#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>

#include <freertos/event_groups.h>

#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_event.h>

#include <espWifi.h>
#include <esp_wpa2.h>

const EventBits_t SCAN_BIT = BIT0;
const EventBits_t STRT_BIT = BIT1;
const EventBits_t DISC_BIT = BIT2;
const EventBits_t DHCP_BIT = BIT3;

#define TAG "espWifi"

void espWifi::event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    EventGroupHandle_t evg = (EventGroupHandle_t)arg;

    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                xEventGroupSetBits(evg, STRT_BIT);
                break;

            case WIFI_EVENT_SCAN_DONE:
                xEventGroupSetBits(evg, SCAN_BIT);
                break;

            case WIFI_EVENT_STA_CONNECTED:
                break;

            case WIFI_EVENT_STA_STOP:
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                xEventGroupSetBits(evg, DISC_BIT);
                break;

            default:
                ESP_LOGW(TAG, "%s: Unhandled event (%" PRIi32 ")", __func__, event_id);
                break;
        }
    }

    if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
            case IP_EVENT_STA_GOT_IP:
                xEventGroupSetBits(evg, DHCP_BIT);
                break;

            default:
                ESP_LOGW(TAG, "%s: Unhandled event (%" PRIi32 ")", __func__, event_id);
                break;
        }
    }
}

esp_err_t espWifi::Init(char *hostname)
{
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    hTask = nullptr;
    evg = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, evg, &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, evg, &instance_got_ip));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    esp_netif_t *myNetIF = esp_netif_create_default_wifi_sta();

    if (hostname)
        esp_netif_set_hostname(myNetIF, hostname);

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    return ESP_OK;
}

void espWifi::init_scan_conf(wifi_scan_config_t *cfg)
{
    cfg->ssid = NULL;
    cfg->bssid = NULL;
    cfg->channel = 0;
    cfg->show_hidden = true;
    cfg->scan_type = WIFI_SCAN_TYPE_ACTIVE;
    cfg->scan_time.active.min = 120;
    cfg->scan_time.active.max = 150;
}

esp_err_t espWifi::Up(void)
{
    uint16_t apCount;
    int i = 0, f;
    static wifi_ap_record_t *ScanList = NULL;
    wifi_config_t wifi_config;
    wifi_scan_config_t scanConf;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    xEventGroupClearBits(evg, STRT_BIT);
    ESP_ERROR_CHECK(esp_wifi_start());

    bool WifiConfigFound;
    int state = 0;
    int done = 0;

    while (!done)
    {
        WifiConfigFound = false;

        switch (state)
        {
            case 0:
                while (!(xEventGroupWaitBits(evg, STRT_BIT, pdFALSE, pdFALSE, portMAX_DELAY) & STRT_BIT))
                    ;

                xEventGroupClearBits(evg, SCAN_BIT);

                init_scan_conf(&scanConf);

                ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, 1));

                while (!(xEventGroupWaitBits(evg, SCAN_BIT, pdFALSE, pdFALSE, portMAX_DELAY) & SCAN_BIT))
                    ;

                apCount = 0;
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&apCount));
                if (!apCount)
                {
                    break;
                }

                ScanList = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_scan_get_ap_records(&apCount, ScanList));

                __attribute__((__fallthrough__));
            case 1:
                state = 0;

                for (i = 0; i < apCount; i++)
                {
                    if (cbAuth != NULL)
                    {
                        TSSIDAuth auth;
                        auth.mode = ScanList[i].authmode;
                        if (cbAuth(this, (const char *)ScanList[i].ssid, &auth) == ESP_OK)
                        {
                            WifiConfigFound = true;
                            state = 1;

                            if ((auth.mode == WIFI_AUTH_WPA2_PSK) || (auth.mode == WIFI_AUTH_WPA_WPA2_PSK))
                            {
                                strncpy((char *)wifi_config.sta.ssid, (char *)ScanList[i].ssid, 32);
                                strncpy((char *)wifi_config.sta.password, auth.password, 64);
                                break;
                            }

                            if (auth.mode == WIFI_AUTH_WPA2_ENTERPRISE)
                            {
                                strncpy((char *)wifi_config.sta.ssid, (char *)ScanList[i].ssid, 32);
                                ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_identity((unsigned char *)auth.identity, strlen(auth.identity)));
                                ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_username((unsigned char *)auth.username, strlen(auth.username)));
                                ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_set_password((unsigned char *)auth.password, strlen(auth.password)));
                                ESP_ERROR_CHECK(esp_wifi_sta_wpa2_ent_enable());
                                break;
                            }
                        }
                    }
                }

                if (WifiConfigFound != true)
                {
                    if (ScanList)
                        free(ScanList);

                    return ESP_FAIL;
                }

                ESP_LOGI(TAG, "%s: Setting WiFi configuration for SSID [%s]", __func__, (char *)wifi_config.sta.ssid);

                wifi_config.sta.pmf_cfg.capable = false;
                wifi_config.sta.pmf_cfg.required = false;

                esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
                xEventGroupClearBits(evg, DISC_BIT | DHCP_BIT);
                ESP_ERROR_CHECK(esp_wifi_connect());
                ESP_LOGI(TAG, "%s: Waiting for IP address...", __func__);
                state = 2;
                __attribute__((__fallthrough__));
            case 2:
                f = xEventGroupWaitBits(evg, DHCP_BIT | DISC_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
                if (f & DISC_BIT)
                    state = 1;
                done = f & DHCP_BIT;
        }
    }

    if (ScanList != NULL)
        free(ScanList);

    return ESP_OK;
}

void espWifi::Task(void *ptr)
{
    espWifi *e = (espWifi *)ptr;

    e->connected = false;

    while (true)
    {
        if (e->Up() != ESP_OK)
        {
            e->connected = false;
            esp_wifi_stop();

            if (e->cbState != NULL)
                e->cbState(e);

            vTaskDelay(2500 / portTICK_PERIOD_MS);
            continue;
        }

        esp_wifi_set_ps(WIFI_PS_NONE);
        e->connected = true;

        if (e->cbState != NULL)
            e->cbState(e);

        ESP_LOGI(TAG, "waiting for DISC_BIT");
        xEventGroupWaitBits(e->evg, DISC_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        ESP_LOGI(TAG, "DISC_BIT received!");

        esp_wifi_stop();

        e->connected = false;

        if (e->cbState != NULL)
            e->cbState(e);

        vTaskDelay(2500 / portTICK_PERIOD_MS);
    }
}

esp_err_t espWifi::Run(void)
{
    if (xTaskCreate(Task, "WifiTask", 8 * 1024, (void *)this, 5, &hTask) == pdPASS)
        return ESP_OK;

    return ESP_FAIL;
}

esp_err_t espWifi::Stop(void)
{
    if (hTask)
    {
        vTaskDelete(hTask);
        return ESP_OK;
    }

    return ESP_FAIL;
}

void espWifi::SetStateCallback(esp_wifi_state_cb cb) { cbState = cb; }

void espWifi::SetAuthCallback(esp_wifi_auth_cb cb) { cbAuth = cb; }

void espWifi::SetUserPointer(void *ptr) { userptr = ptr; }

void *espWifi::GetUserPointer(void) { return userptr; }

espWifi::espWifi()
{
    userptr = NULL;
    cbState = NULL;
    cbAuth = NULL;
    connected = false;
}
