#pragma once

#include "esp_err.h"
#include "nvs_flash.h"

#include "cJSON.h"

#define CONFIGURATION_NVS_NAMESPACE "_CFG_"
#define CONFIGURATION_NVS_JSONFILE "JS"

#define JS_WIFI   "wifi-ssid"
#define JS_USER   "wifi-username"
#define JS_PASS   "wifi-password"
#define JS_IDENT  "wifi-identity"
#define JS_MBIP   "modbus-ip"
#define JS_MBPORT "modbus-port"
#define JS_MQTT_URI  "mqtt-uri"
#define JS_MQTT_USER "mqtt-user"
#define JS_MQTT_PASS "mqtt-password"

class Configuration
{
  public:
    Configuration();
    ~Configuration();

    esp_err_t Init(bool format=false);
    esp_err_t Load(void);
    esp_err_t Save(void);
    esp_err_t Set(const char *k, const char *v);
    const char *Get(const char *k);

    void Dump(void);
    void InitConsole(void);

    // c++ methods for the repl functions:
    int fnInfo(int argc, char **argv);
    int fnReboot(int argc, char **argv);
    int fnSave(int argc, char **argv);
    int fnWifiConfig(int argc, char **argv);
    int fnMQTTConfig(int argc, char **argv);
    int fnMODBUSConfig(int argc, char **argv);

  private:
    nvs_handle_t hNVS;
    cJSON *jsConfig;

};

// c functions to call the c++ methods:
int _fnInfo(int argc, char **argv);
int _fnReboot(int argc, char **argv);
int _fnSave(int argc, char **argv);
int _fnWifiConfig(int argc, char **argv);
int _fnMQTTConfig(int argc, char **argv);
int _fnMODBUSConfig(int argc, char **argv);