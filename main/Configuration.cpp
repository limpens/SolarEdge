#include <esp_log.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <soc/rtc.h>
#include <freertos/FreeRTOS.h>

#include <esp_console.h>
#include <linenoise/linenoise.h>
#include <argtable3/argtable3.h>

#include "Configuration.h"

#define TAG "Configuration"

static struct
{
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_str *username;
    struct arg_str *identity;
    struct arg_end *end;
} WifiConfigArgs;

static struct
{
    struct arg_str *uri;
    struct arg_str *username;
    struct arg_str *password;
    struct arg_str *topic;
    struct arg_int *frequency;
    struct arg_str *topic_ha;
    struct arg_end *end;
} MQTTConfigArgs;

static struct
{
    struct arg_str *ip;
    struct arg_int *port;
    struct arg_end *end;
} MODBUSConfigArgs;

Configuration *_configuration = nullptr;

Configuration::Configuration()
{
    hNVS = -1;
    jsConfig = nullptr;
    _configuration = this;
}

Configuration::~Configuration() { _configuration = nullptr; }

esp_err_t Configuration::Init(bool format)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        if (format == true)
        {
            ESP_LOGI(TAG, "Erasing NVS");
            ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
            err = nvs_flash_init();
        }
    }

    if (err == ESP_OK)
        err = nvs_open(CONFIGURATION_NVS_NAMESPACE, NVS_READWRITE, &hNVS);

    return err;
}

esp_err_t Configuration::Load(void)
{
    if (hNVS != -1)
    {
        size_t strSize = 0;
        esp_err_t err = nvs_get_str(hNVS, CONFIGURATION_NVS_JSONFILE, NULL, &strSize);

        ESP_LOGI(TAG, "strSize: %d (err: %d)", strSize, err);

        if ((err == ESP_OK) && (strSize != 0))
        {
            char *data = (char *)malloc(strSize);
            if (data)
            {
                err = nvs_get_str(hNVS, CONFIGURATION_NVS_JSONFILE, data, &strSize);

                jsConfig = cJSON_Parse(data);
                free(data);

                if (jsConfig)
                    return ESP_OK;
                else
                {
                    ESP_LOGI(TAG, "Deserialization error");
                    return ESP_FAIL;
                }
            }
        }

        return err;
    }
    else
        ESP_LOGI(TAG, "hNVS == -1");

    return ESP_FAIL;
}

esp_err_t Configuration::Save(void)
{
    char *cstr;

    if (jsConfig == nullptr)
        return ESP_FAIL;

    ESP_LOGI(TAG, "Saving configuration");

    cstr = cJSON_Print(jsConfig);

    esp_err_t err = nvs_set_str(hNVS, CONFIGURATION_NVS_JSONFILE, cstr);
    if (err == ESP_OK)
        err = nvs_commit(hNVS);

    if (cstr)
        free(cstr);

    return ESP_OK;
}

esp_err_t Configuration::Set(const char *k, const char *v)
{
    if (!jsConfig)
        jsConfig = cJSON_CreateObject();

    if (jsConfig)
    {
        cJSON *node = cJSON_GetObjectItem(jsConfig, k);

        if (!node)
        {
            cJSON *itemValue = cJSON_CreateString(v);
            cJSON_AddItemToObject(jsConfig, k, itemValue);
        }
        else
        {
            cJSON *itemValue = cJSON_CreateString(v);
            cJSON_ReplaceItemInObject(jsConfig, k, itemValue);
        }

        return ESP_OK;
    }

    return ESP_FAIL;
}

const char *Configuration::Get(const char *k)
{
    if (jsConfig)
    {
        cJSON *node = cJSON_GetObjectItem(jsConfig, k);

        if (node)
            return node->valuestring;
    }

    return nullptr;
}

void Configuration::Dump(void)
{
    ESP_LOGI(TAG, "cJSON Dump start ----------------");

    cJSON *node = jsConfig->child;
    int n = 0;
    while (node)
    {
        const char *key = node->string;
        ESP_LOGI(TAG, "item #%d key: [%s] value: [%s]", n++, key, cJSON_GetStringValue(node));

        node = node->next;
    }
    ESP_LOGI(TAG, "------------------ cJSON Dump end");
}

void Configuration::InitConsole(void)
{
    esp_console_repl_t *repl = nullptr;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    const esp_console_cmd_t cmdReboot = { .command = "reboot", .help = "Reboot to start with saved parameters.", .hint = nullptr, .func = &_fnReboot, .argtable = nullptr };

    const esp_console_cmd_t cmdReset = { .command = "reset", .help = "Reset configuration.", .hint = nullptr, .func = &_fnReset, .argtable = nullptr };

    const esp_console_cmd_t cmdInfo = { .command = "info", .help = "Show information.", .hint = nullptr, .func = &_fnInfo, .argtable = nullptr };

    const esp_console_cmd_t cmdConfWifi = { .command = "wifi", .help = "Configure wifi network.", .hint = nullptr, .func = &_fnWifiConfig, .argtable = &WifiConfigArgs };

    const esp_console_cmd_t cmdConfMQTT = { .command = "mqtt", .help = "Configure MQTT.", .hint = nullptr, .func = &_fnMQTTConfig, .argtable = &MQTTConfigArgs };

    const esp_console_cmd_t cmdConfMODBUS
        = { .command = "modbus", .help = "Configure MODBUS-TCP address.", .hint = nullptr, .func = &_fnMODBUSConfig, .argtable = &MODBUSConfigArgs };

    const esp_console_cmd_t cmdSave
        = { .command = "save", .help = "Save configuration, after configuring wifi, modbus and mqtt parameters.", .hint = nullptr, .func = &_fnSave, .argtable = nullptr };

    WifiConfigArgs.ssid = arg_str1("s", "ssid", "<wifi-ssd>", "Wifi SSD");
    WifiConfigArgs.password = arg_str0("p", "password", "<password>", "Wifi password");
    WifiConfigArgs.username = arg_str0("u", "username", "<username>", "Username when using WPA2 Enterprise");
    WifiConfigArgs.identity = arg_str0("i", "identity", "<identity>", "Identity when using WPA2 Enterprise");
    WifiConfigArgs.end = arg_end(4);

    MQTTConfigArgs.uri = arg_str1("m", "mqtt-uri", "<uri to mqtt broker>", "URI to MQTT broker: mqtt://192.168.193.169:1883");
    MQTTConfigArgs.username = arg_str0("u", "user", "<mqtt user>", "Username for MQTT connection");
    MQTTConfigArgs.password = arg_str0("p", "password", "<mqtt password>", "Password for MQTT connection");
    MQTTConfigArgs.topic = arg_str0("t", "topic", "<mqtt topic>", "topic for the mqtt message. Default: solaredge");
    MQTTConfigArgs.frequency = arg_int0("f", "frequency", "<seconds>", "number of seconds between MQTT messages. Default: 1");
    MQTTConfigArgs.topic_ha = arg_str0("h", "HA-topic", "<mqtt HA topic>", "topic for home-assistant config messages. Default: homeassistant");
    MQTTConfigArgs.end = arg_end(3);

    MODBUSConfigArgs.ip = arg_str1("i", "ip", "<ip address>", "IP address to SolarEdge inverter");
    MODBUSConfigArgs.port = arg_int0("p", "port", "<port number>", "port number for modbus connection. Default: 1502");
    MODBUSConfigArgs.end = arg_end(2);

    repl_config.prompt = "CFG>";
    repl_config.max_cmdline_length = 128;

    esp_console_register_help_command();

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmdReboot));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmdReset));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmdInfo));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmdSave));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmdConfWifi));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmdConfMQTT));
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmdConfMODBUS));

    // use the supplied uart / repl task:
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    // clear screen:
    linenoiseClearScreen();

    printf(LOG_COLOR(LOG_COLOR_CYAN) "+--------------------------------------------+\n");
    printf(LOG_COLOR(LOG_COLOR_CYAN) "|           SolarEdge mqtt gateway           |\n");
    printf(LOG_COLOR(LOG_COLOR_CYAN) "+--------------------------------------------+\n");

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

int _fnReboot(int argc, char **argv)
{
    if (_configuration)
        return _configuration->fnReboot(argc, argv);
    return EXIT_FAILURE;
}

int Configuration::fnReboot(int argc, char **argv)
{
    esp_restart();

    return 0;
}

int _fnReset(int argc, char **argv)
{
    if (_configuration)
        return _configuration->fnReset(argc, argv);
    return EXIT_FAILURE;
}

int Configuration::fnReset(int argc, char **argv)
{
    Set(JS_WIFI, "");
    Set(JS_USER, "");
    Set(JS_PASS, "");
    Set(JS_IDENT, "");

    Set(JS_MBIP, "");
    Set(JS_MBPORT, "");

    Set(JS_MQTT_URI, "");
    Set(JS_MQTT_USER, "");
    Set(JS_MQTT_PASS, "");
    Set(JS_MQTT_TOPIC, "");
    Set(JS_MQTT_FREQ, "");
    Set(JS_MQTT_TOPIC_HA, "");

    printf(LOG_COLOR(LOG_COLOR_RED) "Configuration reset to defaults\n");

    return fnSave(0, nullptr);
}

int _fnInfo(int argc, char **argv)
{
    if (_configuration)
        return _configuration->fnInfo(argc, argv);
    else
        return EXIT_FAILURE;
}

int Configuration::fnInfo(int argc, char **argv)
{
    rtc_cpu_freq_config_t conf;
    uint8_t mac_buf[8];

    rtc_clk_cpu_freq_get_config(&conf);

    esp_netif_ip_info_t ipInfo;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_get_ip_info(netif, &ipInfo);

    esp_read_mac(mac_buf, ESP_MAC_WIFI_STA);

    printf(LOG_COLOR(LOG_COLOR_BLUE) "SDK version:    " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", esp_get_idf_version());
    printf(LOG_COLOR(LOG_COLOR_BLUE) "Application:    " LOG_COLOR(LOG_COLOR_GREEN) _PROJECT_NAME_ "\n");
    printf(LOG_COLOR(LOG_COLOR_BLUE) "Version:        " LOG_COLOR(LOG_COLOR_GREEN) _PROJECT_VER_ "\n");
    printf(LOG_COLOR(LOG_COLOR_BLUE) "CPU Clock:      " LOG_COLOR(LOG_COLOR_GREEN) "%" PRIu32 " Mhz\n", conf.freq_mhz);
    printf(LOG_COLOR(LOG_COLOR_BLUE) "Heap internal:  " LOG_COLOR(LOG_COLOR_GREEN) "%" PRIu16 " bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "Heap default:   " LOG_COLOR(LOG_COLOR_GREEN) "%" PRIu16 " bytes\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "Heap DMA:       " LOG_COLOR(LOG_COLOR_GREEN) "%" PRIu16 " bytes\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "Total heap free:" LOG_COLOR(LOG_COLOR_GREEN) "%" PRIu32 " bytes\n", esp_get_free_heap_size());

    printf(LOG_COLOR(LOG_COLOR_BLUE) "MAC address:    " LOG_COLOR(LOG_COLOR_GREEN) MACSTR "\n", MAC2STR(mac_buf));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "IP address:     " LOG_COLOR(LOG_COLOR_GREEN) IPSTR "\n", IP2STR(&ipInfo.ip));

    printf(LOG_COLOR(LOG_COLOR_RED) "\nCurrent configuration:\n");
    printf(LOG_COLOR(LOG_COLOR_BLUE) "WIFI-SSID:            " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", Get(JS_WIFI));
    // printf(LOG_COLOR(LOG_COLOR_BLUE) "WIFI-Password:        " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", Get(JS_PASS));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "WIFI-User (wpa2):     " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", Get(JS_USER));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "WIFI-Identity (wpa2): " LOG_COLOR(LOG_COLOR_GREEN) "%s\n\n", Get(JS_IDENT));

    printf(LOG_COLOR(LOG_COLOR_BLUE) "MODBUS IP:            " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", Get(JS_MBIP));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "MODBUS port:          " LOG_COLOR(LOG_COLOR_GREEN) "%s\n\n", Get(JS_MBPORT));

    printf(LOG_COLOR(LOG_COLOR_BLUE) "MQTT URI:                 " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", Get(JS_MQTT_URI));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "MQTT topic:               " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", Get(JS_MQTT_TOPIC));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "MQTT Homeassistant topic: " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", Get(JS_MQTT_TOPIC_HA));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "MQTT user:                " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", Get(JS_MQTT_USER));
    printf(LOG_COLOR(LOG_COLOR_BLUE) "MQTT frequency:           " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", Get(JS_MQTT_FREQ));
    // printf(LOG_COLOR(LOG_COLOR_BLUE) "MQTT password: " LOG_COLOR(LOG_COLOR_GREEN) "%s\n", Get(JS_MQTT_PASS));

    return 0;
}

int _fnWifiConfig(int argc, char **argv)
{
    if (_configuration)
        return _configuration->fnWifiConfig(argc, argv);
    else
        return EXIT_FAILURE;
}

int Configuration::fnWifiConfig(int argc, char **argv)
{
    int n = arg_parse(argc, argv, (void **)&WifiConfigArgs);
    if (n != 0)
    {
        printf("\nError in arguments. Type 'help' for info.\n");
        return EXIT_FAILURE;
    }

    const char *ssid = WifiConfigArgs.ssid->count ? WifiConfigArgs.ssid->sval[0] : nullptr;
    const char *password = WifiConfigArgs.password->count ? WifiConfigArgs.password->sval[0] : nullptr;
    const char *username = WifiConfigArgs.username->count ? WifiConfigArgs.username->sval[0] : nullptr;
    const char *identity = WifiConfigArgs.identity->count ? WifiConfigArgs.identity->sval[0] : nullptr;

    Set(JS_WIFI, ssid);
    Set(JS_USER, username);
    Set(JS_IDENT, identity);
    Set(JS_PASS, password);

    return 0;
}

int _fnMQTTConfig(int argc, char **argv)
{
    if (_configuration)
        return _configuration->fnMQTTConfig(argc, argv);
    else
        return EXIT_FAILURE;
}

int Configuration::fnMQTTConfig(int argc, char **argv)
{
    char buf[8];

    int n = arg_parse(argc, argv, (void **)&MQTTConfigArgs);
    if (n != 0)
    {
        printf("\n" LOG_COLOR(LOG_COLOR_RED) "Error in arguments. Type 'help' for info.\n");
        return EXIT_FAILURE;
    }

    const char *uri = MQTTConfigArgs.uri->count ? MQTTConfigArgs.uri->sval[0] : nullptr;
    const char *user = MQTTConfigArgs.username->count ? MQTTConfigArgs.username->sval[0] : nullptr;
    const char *pass = MQTTConfigArgs.password->count ? MQTTConfigArgs.password->sval[0] : nullptr;
    const char *topic = MQTTConfigArgs.topic->count ? MQTTConfigArgs.topic->sval[0] : "solaredge";
    uint16_t freq = MQTTConfigArgs.frequency->count ? MQTTConfigArgs.frequency->ival[0] : 1;
    const char *topic_ha = MQTTConfigArgs.topic_ha->count ? MQTTConfigArgs.topic_ha->sval[0] : "homeassistant";

    if (freq == 0)
        freq = 1;

    snprintf(buf, sizeof(buf), "%" PRIu16, freq);
    Set(JS_MQTT_URI, uri);
    Set(JS_MQTT_USER, user);
    Set(JS_MQTT_PASS, pass);
    Set(JS_MQTT_TOPIC, topic);
    Set(JS_MQTT_FREQ, buf);
    Set(JS_MQTT_TOPIC_HA, topic_ha);

    return 0;
}

int _fnMODBUSConfig(int argc, char **argv)
{
    if (_configuration)
        return _configuration->fnMODBUSConfig(argc, argv);
    else
        return EXIT_FAILURE;
}

int Configuration::fnMODBUSConfig(int argc, char **argv)
{
    char buf[8];

    int n = arg_parse(argc, argv, (void **)&MODBUSConfigArgs);
    if (n != 0)
    {
        printf("\n" LOG_COLOR(LOG_COLOR_RED) "Error in arguments. Type 'help' for info.\n");
        return EXIT_FAILURE;
    }

    const char *ip = MODBUSConfigArgs.ip->count ? MODBUSConfigArgs.ip->sval[0] : nullptr;
    uint16_t port = MODBUSConfigArgs.port->count ? MODBUSConfigArgs.port->ival[0] : 1502;
    snprintf(buf, sizeof(buf), "%" PRIu16, port);

    Set(JS_MBIP, ip);
    Set(JS_MBPORT, buf);

    return 0;
}

int _fnSave(int argc, char **argv)
{
    if (_configuration)
        return _configuration->fnSave(argc, argv);
    else
        return EXIT_FAILURE;
}

int Configuration::fnSave(int argc, char **argv)
{
    if (Save() == ESP_OK)
        printf(LOG_COLOR(LOG_COLOR_RED) "\nConfiguration saved. Reboot to activate\n");

    return 0;
}
