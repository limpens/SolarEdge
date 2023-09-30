#include <stdio.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_err.h>
#include <esp_netif.h>
#include <mqtt_client.h>
#include <esp_sntp.h>

#include "sdkconfig.h"

#ifdef CONFIG_SOLAREDGE_USE_LCD
#include <driver/gpio.h>
#include <driver/ledc.h>

#include "lvgl.h"

#include "lcd.h"
#include "gui.h"
#endif // CONFIG_SOLAREDGE_USE_LCD

#include "wifi.h"
#include "modbus.h"
#include "TaskModbus.h"
#include "Configuration.h"
#include "solaredge_mqtt.h"
#include "private_types.h"

#define TAG _PROJECT_NAME_

bool NTPTimeSynced = false;
static WifiUser_t wifiUser;

#ifdef CONFIG_SOLAREDGE_USE_LCD
typedef struct
{
    modbus *mb;
    GuiData_t *gui;
} TaskGuiUpdate_t;
#endif

static void time_sync_notification_cb(struct timeval *tv)
{
    static char buf[24];

    struct tm *ltm = localtime(&tv->tv_sec);
    strftime(buf, sizeof(buf), "%Y/%m/%d %X", ltm);

    ESP_LOGI(TAG, "Time synced using NTP at %s ", buf);

    NTPTimeSynced = true;
}

#ifdef CONFIG_SOLAREDGE_USE_LCD
void TaskGuiStatusUpdate(void *param)
{
    TaskGuiUpdate_t *data = (TaskGuiUpdate_t *)param;

    while (true)
    {
        if (wifiUser.online)
        {
            if (data->mb->is_connected())
                GUI_SetStatus(data->gui, GUI_STATE_TIME);
            else
                GUI_SetStatus(data->gui, GUI_STATE_SE_DISCONNECTED);
        }
        else
        {
            GUI_SetStatus(data->gui, GUI_STATE_WIFI_DISCONNECT);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void TaskLcdBackLight(void *param)
{
    GuiData_t *gd = (GuiData_t *)param;
    uint8_t pct;

    ledc_timer_config_t ledc_timer = { .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK /*, .deconfigure = false */ };

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = { .gpio_num = LCD_PIN_BK_LIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags = { .output_invert = 1 } };

    if (ledc_channel_config(&ledc_channel) == ESP_OK)
    {
        while (42)
        {
            if (xSemaphoreTake(gd->BackLightChange, portMAX_DELAY))
            {
                if (gd->BackLightActive)
                    pct = 85;
                else
                    pct = 15;

                uint32_t dutyCycle = (8191 / 100) * (100 - pct);

                ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, dutyCycle));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
            }
        }
    }

    vTaskDelete(NULL);
}
#endif

extern "C" void app_main(void)
{
#ifdef CONFIG_SOLAREDGE_USE_LCD
    static TaskGuiUpdate_t dataGui;
    GuiData_t GuiData;
#endif // CONFIG_SOLAREDGE_USE_LCD

    static modbus mb = modbus();
    uint16_t modbusPort = 0;
    static SolarEdgeSunSpec_t sunspec;
    static SolarEdge_t solaredge;
    static TaskModbus_t data;
    esp_mqtt_client_config_t mqtt_cfg;
    esp_mqtt_client_handle_t mqtt_client = nullptr;
    uint16_t mqtt_freq = 0;
    Configuration config;
    int lastDOW;
    static MQTT_user_t mqtt_user;

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    char buf[1024];
#endif

    data.lock = xSemaphoreCreateBinary();
    data.mb = &mb;
    data.solaredge = &solaredge;
    data.sunspec = &sunspec;

    solaredge.I_AC_Energy_WH_Last24H = 0;
    lastDOW = -1;

#if 1
    esp_log_level_set("pp", ESP_LOG_ERROR);
    esp_log_level_set("net80211", ESP_LOG_ERROR);
    esp_log_level_set("wifi_init", ESP_LOG_ERROR);
    esp_log_level_set("phy_init", ESP_LOG_ERROR);
    esp_log_level_set("gpio", ESP_LOG_ERROR);
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_ERROR);

    esp_log_level_set("WiFi", ESP_LOG_ERROR);
    esp_log_level_set("espWifi", ESP_LOG_ERROR);
    esp_log_level_set("GT911", ESP_LOG_ERROR);
    esp_log_level_set("LCD", ESP_LOG_ERROR);
    esp_log_level_set("PublishMQTT", ESP_LOG_ERROR);
    esp_log_level_set("dhcpc", ESP_LOG_INFO);
#endif

    config.Init(true);
    if (config.Load() != ESP_OK)
    {
        ESP_LOGI(TAG, "Missing configuration");

        config.fnReset(0, nullptr);
    }
    else
    {
        ESP_LOGI(TAG, "Configuration:");
        config.Dump();
    }

    config.InitConsole();

#ifdef CONFIG_SOLAREDGE_USE_LCD

    ESP_ERROR_CHECK(LCDInit());

    GuiData.BackLightChange = xSemaphoreCreateBinary();
    GuiData.BackLightActive = true;
    GuiData.ntp_synced = &NTPTimeSynced;

    GUI_Setup(&GuiData);

    if (xTaskCreate(TaskLcdBackLight, "LCDBackLight", configMINIMAL_STACK_SIZE * 2, &GuiData, 4, nullptr) != pdPASS)
        ESP_LOGE(TAG, "xTaskCreate( TaskLcdBackLight ): failed");

    xSemaphoreGive(GuiData.BackLightChange);

    GUI_SetStatus(&GuiData, GUI_STATE_WIFI_DISCONNECT);

    dataGui.gui = &GuiData;
    dataGui.mb = &mb;

    if (xTaskCreate(TaskGuiStatusUpdate, "GuiStatus", configMINIMAL_STACK_SIZE * 4, &dataGui, 4, nullptr) != pdPASS)
        ESP_LOGE(TAG, "xTaskCreate( TaskGuiStatusUpdate ): failed");
#endif

    wifiUser.online = false;
    wifiUser.ssid = strdup(config.Get(JS_WIFI));
    wifiUser.password = strdup(config.Get(JS_PASS));
    wifiUser.wpa2_user = strdup(config.Get(JS_USER));
    wifiUser.wpa2_ident = strdup(config.Get(JS_IDENT));

    ESP_ERROR_CHECK(WifiStart(&wifiUser));

    ESP_LOGI(TAG, "Waiting for wifi connection..");
    while (wifiUser.online == false)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "IP address configured.");

    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    esp_sntp_servermode_dhcp(true);
    setenv("TZ", "CET-1CEST,M3.5.0/02:00:00,M10.5.0/02:00:00", 1); // Netherlands/Amsterdam
    tzset();

#ifdef CONFIG_SOLAREDGE_USE_LCD
    GUI_SetStatus(&GuiData, GUI_STATE_WIFI_CONNECTED);
#endif

    sscanf(config.Get(JS_MBPORT), "%" PRIu16, &modbusPort);
    mb.SetHost(config.Get(JS_MBIP), modbusPort);

    mb.SetSlaveID(1);

    if (xTaskCreate(TaskModbus, "Modbus", configMINIMAL_STACK_SIZE * 4, &data, 4, nullptr) != pdPASS)
        ESP_LOGE(TAG, "xTaskCreate( TaskModbus ): failed");

    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));

    const char *mqttHost = config.Get(JS_MQTT_URI);
    const char *mqttUser = config.Get(JS_MQTT_USER);
    const char *mqttPass = config.Get(JS_MQTT_PASS);
    const char *mqttTopic = config.Get(JS_MQTT_TOPIC);
    const char *mqttTopicHA = config.Get(JS_MQTT_TOPIC_HA);

    sscanf(config.Get(JS_MQTT_FREQ), "%" PRIu16, &mqtt_freq);

    if (mqttHost)
    {
        mqtt_cfg.broker.address.uri = mqttHost;
        mqtt_cfg.credentials.client_id = NULL;
        mqtt_cfg.credentials.username = mqttUser;
        mqtt_cfg.credentials.authentication.password = mqttPass;

        ESP_LOGI(TAG, "MQTT host: %s", mqtt_cfg.broker.address.uri);

        mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
        if (esp_mqtt_client_start(mqtt_client) == ESP_OK)
        {
            mqtt_user.mqtt_client = mqtt_client;
            mqtt_user.mqtt_ha_topic = mqttTopicHA;
            mqtt_user.mqtt_topic = mqttTopic;

            esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_CONNECTED, mqtt_event_handler, &mqtt_user);
            ESP_LOGI(TAG, "MQTT client started");
        }
        else
        {
            mqtt_client = NULL;
            ESP_LOGI(TAG, "MQTT client failed to start");
        }
    }

    while (true)
    {
        if (xSemaphoreTake(data.lock, portMAX_DELAY))
        {
#ifdef CONFIG_SOLAREDGE_USE_LCD
            static time_t bl_timer = time(NULL) + 1;
            if (bl_timer <= time(NULL) && GuiData.BackLightActive)
            {
                bl_timer = time(NULL) + 1;
                GuiData.BackLightActive--;

                if (!GuiData.BackLightActive)
                    xSemaphoreGive(GuiData.BackLightChange);
            }

            GUI_UpdatePanels(&GuiData, data.solaredge);
#endif // CONFIG_SOLAREDGE_USE_LCD

            static time_t mqtt_timer = time(NULL) + mqtt_freq;
            if (mqtt_timer <= time(NULL))
            {
                mqtt_timer = time(NULL) + mqtt_freq;
                PublishMQTT(&mqtt_user, &data);
            }

            time_t t = time(NULL);
            struct tm *ltm = localtime(&t);

            if (ltm->tm_mday != lastDOW)
            {
                lastDOW = ltm->tm_mday;
                solaredge.I_AC_Energy_WH_Last24H = solaredge.I_AC_Energy_WH;
            }
        }

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
        vTaskGetRunTimeStats(buf);
        printf("\n%s\n", buf);
        printf("HEAP: %" PRIi32 "\n", esp_get_free_heap_size());
        printf("INTERNAL: %" PRIi32 "\n", esp_get_free_internal_heap_size());
#endif
    }
}
