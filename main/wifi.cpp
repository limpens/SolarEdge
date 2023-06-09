#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include <string.h> // strcmp

#include "espWifi.h"

#include "wifi.h"

#include "sdkconfig.h"

#define TAG "WiFi"

void WifiStateCallback(espWifi *e)
{
//bool *b = (bool*)e->GetUserPointer();
WifiUser_t *userptr = (WifiUser_t*)e->GetUserPointer();

  if (e->connected)
  {
    ESP_LOGI(TAG, "Wifi connected");
    userptr->online = true;
  }
  else
  {
    ESP_LOGI(TAG, "Wifi disconnected");
    userptr->online = false;
  }
}

esp_err_t WifiAuthCallback(espWifi *e, const char *ssid, TSSIDAuth *authInfo)
{
WifiUser_t *userptr = (WifiUser_t*)e->GetUserPointer();

  ESP_LOGI(TAG, "In auth callback, looking for credentials for the network [%s]", ssid);

  //if ((!strcmp(ssid, CONFIG_SOLAREDGE_WIFI_AP_SSID)) && ((authInfo->mode == WIFI_AUTH_WPA2_PSK) || (authInfo->mode == WIFI_AUTH_WPA_WPA2_PSK)))
  if ((!strcmp(ssid, userptr->ssid)) && ((authInfo->mode == WIFI_AUTH_WPA2_PSK) || (authInfo->mode == WIFI_AUTH_WPA_WPA2_PSK)))
  {
    authInfo->password = userptr->password;
    return ESP_OK;
  }

  if (!strcmp(ssid, userptr->ssid) && (authInfo->mode == WIFI_AUTH_WPA2_ENTERPRISE))
  {
    authInfo->identity = userptr->wpa2_ident;
    authInfo->password = userptr->password;
    authInfo->username = userptr->wpa2_user;

    return ESP_OK;
  }

  return ESP_FAIL;
}


esp_err_t WifiStart(void *userptr)
{
static espWifi WifiHandler;

  ESP_ERROR_CHECK(WifiHandler.Init((char*)"ESP32-" _PROJECT_NAME_));

  WifiHandler.SetUserPointer(userptr);
  WifiHandler.SetStateCallback( WifiStateCallback );
  WifiHandler.SetAuthCallback( WifiAuthCallback );

  return WifiHandler.Run();
}