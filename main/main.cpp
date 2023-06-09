#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "esp_sntp.h"

#include "sdkconfig.h"

#ifdef CONFIG_SOLAREDGE_USE_LCD
#include "driver/gpio.h"
#include "lvgl.h"

#include "lcd.h"
#include "gui.h"
#endif // CONFIG_SOLAREDGE_USE_LCD

#include "wifi.h"
#include "modbus.h"
#include "TaskModbus.h"
#include "Configuration.h"

#define TAG _PROJECT_NAME_

bool NTPTimeSynced = false;
static WifiUser_t wifiUser;

#ifdef CONFIG_SOLAREDGE_USE_LCD
typedef struct
{
  modbus *mb;
  GuiData_t *gui;
}TaskGuiUpdate_t;
#endif

//
// callback function for sntp_set_time_sync_notification_cb() so we can
// register if we've got a properly synchronized clock
//
static void time_sync_notification_cb(struct timeval *tv)
{
static char buf[24];

  struct tm *ltm = localtime(&tv->tv_sec);
  strftime(buf, sizeof(buf), "%Y/%m/%d %X", ltm);
 
  ESP_LOGI(TAG, "Time synced using NTP at %s ", buf);

  NTPTimeSynced = true;
}

void TaskGuiStatusUpdate(void *param)
{
  #ifdef CONFIG_SOLAREDGE_USE_LCD
  TaskGuiUpdate_t *data = (TaskGuiUpdate_t*)param;
  
  while(true)
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
  #endif // CONFIG_SOLAREDGE_USE_LCD
}
//
// - Initialize wifi and the modbus class
// - Setup the display hardware and lvgl
//   - wait for a wifi connection & ip address
//   - start the ntp cycle and update the display with the current state
//   - start the task for modbus-tcp communication
//   - connect to mqtt broker
//
//   Wait for the semphore indicating new modbus data has been parsed/converted and
//   update the display-objects + create a json document to be published to the broker
//
extern "C" void app_main(void)
{
#ifdef CONFIG_SOLAREDGE_USE_LCD
static TaskGuiUpdate_t dataGui;
GuiData_t GuiData;
GuiData.GuiLock = xSemaphoreCreateBinary();
#endif // CONFIG_SOLAREDGE_USE_LCD

static modbus mb = modbus(); //(char *)CONFIG_SOLAREDGE_IP, CONFIG_SOLAREDGE_PORT);
static SolarEdgeSunSpec_t sunspec;
static SolarEdge_t solaredge;
static TaskModus_t data;
esp_mqtt_client_config_t mqtt_cfg;
esp_mqtt_client_handle_t mqtt_client = nullptr;
cJSON *jsDOC = nullptr;
Configuration config;


#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
char buf[1024];
#endif

  //
  // setup data structure passed to the modbus task.
  //
  data.lock = xSemaphoreCreateBinary();
  data.mb = &mb;
  data.solaredge = &solaredge;
  data.sunspec = &sunspec;


  //
  // change the loglevels:
  //
  #if 1
  //esp_log_level_set("*", ESP_LOG_ERROR);
  esp_log_level_set("espWifi", ESP_LOG_ERROR);
  esp_log_level_set("WiFi", ESP_LOG_ERROR);
  esp_log_level_set("wifi", ESP_LOG_ERROR);
  esp_log_level_set("wifi_init", ESP_LOG_ERROR);
  esp_log_level_set("phy_init", ESP_LOG_ERROR);
  esp_log_level_set("dhcpc", ESP_LOG_INFO);
  #endif

  //
  // Initialize config class
  //
  config.Init(true);
  if (config.Load() != ESP_OK)
  {
    ESP_LOGI(TAG, "Missing configuration");

    config.Set(JS_WIFI, "");
    config.Set(JS_USER, "");
    config.Set(JS_PASS, "");
    config.Set(JS_IDENT, "");

    config.Set(JS_MBIP, "");
    config.Set(JS_MBPORT, "");

    config.Set(JS_MQTT_URI, "");
    config.Set(JS_MQTT_USER, "");
    config.Set(JS_MQTT_PASS, "");

    config.Save();
  }
  else
  {
    ESP_LOGI(TAG, "Configuration:");
    config.Dump();
  }

  config.InitConsole();

#ifdef CONFIG_SOLAREDGE_USE_LCD
  //
  // initialize display, i2c and touch handler:
  //
  ESP_ERROR_CHECK(LCDInit(GuiData.GuiLock));

  //
  // Create lvgl objects:
  //
  GUI_Setup(&GuiData);

  //
  // turn on lcd backlight, this prevents displaying noise during initialization of the display
  //
  gpio_set_level(LCD_PIN_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL);
  GUI_SetStatus(&GuiData, GUI_STATE_WIFI_DISCONNECT);

  dataGui.gui = &GuiData;
  dataGui.mb = &mb;

  if (xTaskCreate(TaskGuiStatusUpdate, "GuiStatus", configMINIMAL_STACK_SIZE * 4, &dataGui, 4, nullptr) != pdPASS)
    ESP_LOGE(TAG, "xTaskCreate( TaskGuiStatusUpdate ): failed");  
#endif


  //
  // setup Wifi
  //
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

  //
  // configure clock using SNTP:
  //
  esp_sntp_setservername(0, "pool.ntp.org"); //TODO add ntp server only when not configured by dhcp
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  sntp_set_time_sync_notification_cb(time_sync_notification_cb);
  esp_sntp_init();
  setenv("TZ", "CET-1CEST,M3.5.0/02:00:00,M10.5.0/02:00:00", 1);
  tzset();

  #ifdef CONFIG_SOLAREDGE_USE_LCD
  GUI_SetStatus(&GuiData, GUI_STATE_WIFI_CONNECTED);
  #endif

  //
  // Start task for the modbus code, supply pointer to modbus class and internal structures
  //
  uint16_t modbusPort=0;
  sscanf(config.Get(JS_MBPORT), "%" PRIu16, &modbusPort);
  mb.SetHost(config.Get(JS_MBIP), modbusPort);
  
  mb.SetSlaveID(1);

  if (xTaskCreate(TaskModbus, "Modbus", configMINIMAL_STACK_SIZE * 4, &data, 4, nullptr) != pdPASS)
    ESP_LOGE(TAG, "xTaskCreate( TaskModbus ): failed");

  ESP_LOGI(TAG, "Running..");

  //
  // Start MQTT connection
  //
  memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));

  const char *mqttHost = config.Get(JS_MQTT_URI);
  const char *mqttUser = config.Get(JS_MQTT_USER);
  const char *mqttPass = config.Get(JS_MQTT_PASS);

  if (mqttHost)
  {
    mqtt_cfg.broker.address.uri = mqttHost;
    mqtt_cfg.credentials.client_id = NULL; // let library create a unique id
    mqtt_cfg.credentials.username = mqttUser;
    mqtt_cfg.credentials.authentication.password = mqttPass;

    ESP_LOGI(TAG, "MQTT host: %s", mqtt_cfg.broker.address.uri);

    mqtt_client  = esp_mqtt_client_init(&mqtt_cfg);
    if (esp_mqtt_client_start(mqtt_client) == ESP_OK)
      ESP_LOGI(TAG, "MQTT client started");
    else
    {
      mqtt_client = NULL;
      ESP_LOGI(TAG, "MQTT client failed to start");
    }
  }


  //
  // The wifi class will try to keep a valid connection
  // The mqtt code will retry to connect
  // the modbus class will attempt to reconnect on errors
  // 
  // Wait for the sempahore and update display
  //
  while (true)
  {
    //
    // Semaphore is triggered by modbus task, when new values are received _and_ converted:
    if (xSemaphoreTake(data.lock, portMAX_DELAY))
    {
      #ifdef CONFIG_SOLAREDGE_USE_LCD
      GUI_UpdatePanels(&GuiData, data.solaredge);
      #endif // CONFIG_SOLAREDGE_USE_LCD

      jsDOC = cJSON_CreateObject();
      if (jsDOC)
      {
        cJSON_AddItemToObject(jsDOC, "esp_uptime", cJSON_CreateNumber(esp_timer_get_time()/(1000*1000))); // convert from microseconds to seconds
        cJSON_AddItemToObject(jsDOC, "C_Manufacturer", cJSON_CreateString((const char*)data.sunspec->C_Manufacturer));
        cJSON_AddItemToObject(jsDOC, "C_Model", cJSON_CreateString((const char*)data.sunspec->C_Model));
        cJSON_AddItemToObject(jsDOC, "C_Version", cJSON_CreateString((const char*)data.sunspec->C_Version));
        cJSON_AddItemToObject(jsDOC, "C_SerialNumber", cJSON_CreateString((const char*)data.sunspec->C_SerialNumber));
        cJSON_AddItemToObject(jsDOC, "C_SunSpec_Phase", cJSON_CreateNumber(data.sunspec->C_SunSpec_Phase));

        cJSON_AddItemToObject(jsDOC, "I_AC_Current",cJSON_CreateNumber(data.solaredge->I_AC_Current));
        cJSON_AddItemToObject(jsDOC, "I_AC_CurrentA",cJSON_CreateNumber(data.solaredge->I_AC_CurrentA));
        cJSON_AddItemToObject(jsDOC, "I_AC_CurrentB",cJSON_CreateNumber(data.solaredge->I_AC_CurrentB));
        cJSON_AddItemToObject(jsDOC, "I_AC_CurrentC",cJSON_CreateNumber(data.solaredge->I_AC_CurrentC));

        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageAB",cJSON_CreateNumber(data.solaredge->I_AC_VoltageAB));
        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageBC",cJSON_CreateNumber(data.solaredge->I_AC_VoltageBC));
        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageCA",cJSON_CreateNumber(data.solaredge->I_AC_VoltageCA));

        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageAN",cJSON_CreateNumber(data.solaredge->I_AC_VoltageAN));
        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageBN",cJSON_CreateNumber(data.solaredge->I_AC_VoltageBN));
        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageCN",cJSON_CreateNumber(data.solaredge->I_AC_VoltageCN));

        cJSON_AddItemToObject(jsDOC, "I_AC_Power",cJSON_CreateNumber(data.solaredge->I_AC_Power));
        cJSON_AddItemToObject(jsDOC, "I_AC_Frequency",cJSON_CreateNumber(data.solaredge->I_AC_Frequency));
        cJSON_AddItemToObject(jsDOC, "I_AC_VA",cJSON_CreateNumber(data.solaredge->I_AC_VA));
        cJSON_AddItemToObject(jsDOC, "I_AC_VAR",cJSON_CreateNumber(data.solaredge->I_AC_VAR));
        cJSON_AddItemToObject(jsDOC, "I_AC_PF",cJSON_CreateNumber(data.solaredge->I_AC_PF));

        cJSON_AddItemToObject(jsDOC, "I_AC_Energy_WH",cJSON_CreateNumber(data.solaredge->I_AC_Energy_WH));

        cJSON_AddItemToObject(jsDOC, "I_DC_Current",cJSON_CreateNumber(data.solaredge->I_DC_Current));
        cJSON_AddItemToObject(jsDOC, "I_DC_Voltage",cJSON_CreateNumber(data.solaredge->I_DC_Voltage));
        cJSON_AddItemToObject(jsDOC, "I_DC_Power",cJSON_CreateNumber(data.solaredge->I_DC_Power));

        cJSON_AddItemToObject(jsDOC, "I_Temp_Sink",cJSON_CreateNumber(data.solaredge->I_Temp_Sink));

        cJSON_AddItemToObject(jsDOC, "I_Status", cJSON_CreateNumber(data.solaredge->I_Status));
        cJSON_AddItemToObject(jsDOC, "I_Status_Vendor", cJSON_CreateNumber(data.solaredge->I_Status_Vendor));

        // convert json object to string and free-up memory:
        char *strJSON = cJSON_Print(jsDOC);
        cJSON_Delete(jsDOC);
        jsDOC = nullptr;

        // publish strJSON over mqtt and free string:
        if (esp_mqtt_client_publish(mqtt_client, CONFIG_SOLAREDGE_MQTT_TOPIC, strJSON, 0, 0, 0) == -1)
          ESP_LOGI(TAG, "mqtt publish error occurred!");

        free(strJSON);
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
