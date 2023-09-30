#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <mqtt_client.h>
#include <cJSON.h>

#include "modbus.h"
#include "private_types.h"

#include "sdkconfig.h"

#include "solaredge_mqtt.h"

#define TAG "PublishMQTT"

#define MQTT_DEFAULT_TOPIC "solaredge"

HA_ConfigMsg_T HA_CfgSensors[] = {
    { "I_AC_Energy_WH", "i_ac_energy_wh", "energy", "total_increasing", "Wh" }, /* data->solaredge->I_AC_Energy_WH */
    { "I_Temp_Sink", "i_temp_sink", "temperature", "measurement", "°C" }        /* data->solaredge->I_Temp_Sink */
};

esp_err_t PublishMQTT(MQTT_user_t *mqtt_user, TaskModbus_t *data)
{
    char topic_buf[128], message_buf[64];

    cJSON *jsDOC;

    jsDOC = cJSON_CreateObject();
    if (jsDOC)
    {
        cJSON_AddItemToObject(jsDOC, "esp_uptime", cJSON_CreateNumber(esp_timer_get_time() / (1000 * 1000)));
        cJSON_AddItemToObject(jsDOC, "C_Manufacturer", cJSON_CreateString((const char *)data->sunspec->C_Manufacturer));
        cJSON_AddItemToObject(jsDOC, "C_Model", cJSON_CreateString((const char *)data->sunspec->C_Model));
        cJSON_AddItemToObject(jsDOC, "C_Version", cJSON_CreateString((const char *)data->sunspec->C_Version));
        cJSON_AddItemToObject(jsDOC, "C_SerialNumber", cJSON_CreateString((const char *)data->sunspec->C_SerialNumber));
        cJSON_AddItemToObject(jsDOC, "C_SunSpec_Phase", cJSON_CreateNumber(data->sunspec->C_SunSpec_Phase));

        cJSON_AddItemToObject(jsDOC, "I_AC_Current", cJSON_CreateNumber(data->solaredge->I_AC_Current));
        cJSON_AddItemToObject(jsDOC, "I_AC_CurrentA", cJSON_CreateNumber(data->solaredge->I_AC_CurrentA));
        cJSON_AddItemToObject(jsDOC, "I_AC_CurrentB", cJSON_CreateNumber(data->solaredge->I_AC_CurrentB));
        cJSON_AddItemToObject(jsDOC, "I_AC_CurrentC", cJSON_CreateNumber(data->solaredge->I_AC_CurrentC));

        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageAB", cJSON_CreateNumber(data->solaredge->I_AC_VoltageAB));
        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageBC", cJSON_CreateNumber(data->solaredge->I_AC_VoltageBC));
        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageCA", cJSON_CreateNumber(data->solaredge->I_AC_VoltageCA));

        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageAN", cJSON_CreateNumber(data->solaredge->I_AC_VoltageAN));
        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageBN", cJSON_CreateNumber(data->solaredge->I_AC_VoltageBN));
        cJSON_AddItemToObject(jsDOC, "I_AC_VoltageCN", cJSON_CreateNumber(data->solaredge->I_AC_VoltageCN));

        cJSON_AddItemToObject(jsDOC, "I_AC_Power", cJSON_CreateNumber(data->solaredge->I_AC_Power));
        cJSON_AddItemToObject(jsDOC, "I_AC_Frequency", cJSON_CreateNumber(data->solaredge->I_AC_Frequency));
        cJSON_AddItemToObject(jsDOC, "I_AC_VA", cJSON_CreateNumber(data->solaredge->I_AC_VA));
        cJSON_AddItemToObject(jsDOC, "I_AC_VAR", cJSON_CreateNumber(data->solaredge->I_AC_VAR));
        cJSON_AddItemToObject(jsDOC, "I_AC_PF", cJSON_CreateNumber(data->solaredge->I_AC_PF));

        cJSON_AddItemToObject(jsDOC, "I_AC_Energy_WH", cJSON_CreateNumber(data->solaredge->I_AC_Energy_WH));
        cJSON_AddItemToObject(jsDOC, "I_AC_Energy_WH_24H", cJSON_CreateNumber(data->solaredge->I_AC_Energy_WH - data->solaredge->I_AC_Energy_WH_Last24H));

        cJSON_AddItemToObject(jsDOC, "I_DC_Current", cJSON_CreateNumber(data->solaredge->I_DC_Current));
        cJSON_AddItemToObject(jsDOC, "I_DC_Voltage", cJSON_CreateNumber(data->solaredge->I_DC_Voltage));
        cJSON_AddItemToObject(jsDOC, "I_DC_Power", cJSON_CreateNumber(data->solaredge->I_DC_Power));

        cJSON_AddItemToObject(jsDOC, "I_Temp_Sink", cJSON_CreateNumber(data->solaredge->I_Temp_Sink));

        cJSON_AddItemToObject(jsDOC, "I_Status", cJSON_CreateNumber(data->solaredge->I_Status));
        cJSON_AddItemToObject(jsDOC, "I_Status_Vendor", cJSON_CreateNumber(data->solaredge->I_Status_Vendor));

        char *strJSON = cJSON_Print(jsDOC);

        if (esp_mqtt_client_publish(mqtt_user->mqtt_client, mqtt_user->mqtt_topic, strJSON, 0, 0, 0) == -1)
            ESP_LOGI(TAG, "mqtt publish error occurred!");

        for (size_t i = 0; i < sizeof(HA_CfgSensors) / sizeof(HA_CfgSensors[0]); i++)
        {
            cJSON *item = cJSON_GetObjectItem(jsDOC, HA_CfgSensors[i].Name);

            snprintf(topic_buf, sizeof(topic_buf), "%s/%s/state", mqtt_user->mqtt_topic, HA_CfgSensors[i].UniqueId);
            snprintf(message_buf, sizeof(message_buf), "%d", item->valueint);
            esp_mqtt_client_publish(mqtt_user->mqtt_client, topic_buf, message_buf, 0, 0, 0);
        }

        cJSON_Delete(jsDOC);
        jsDOC = nullptr;

        free(strJSON);

        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t HAConfigMessage(MQTT_user_t *mqtt_user)
{
    char topicBuf[128];
    char haTopic[128];

    for (size_t i = 0; i < sizeof(HA_CfgSensors) / sizeof(HA_CfgSensors[0]); i++)
    {
        sprintf(topicBuf, "%s/%s/state", mqtt_user->mqtt_topic, HA_CfgSensors[i].UniqueId);

        cJSON *jsConfig = cJSON_CreateObject();

        cJSON_AddItemToObject(jsConfig, "name", cJSON_CreateString(HA_CfgSensors[i].Name));
        cJSON_AddItemToObject(jsConfig, "unique_id", cJSON_CreateString(HA_CfgSensors[i].UniqueId));
        cJSON_AddItemToObject(jsConfig, "device_class", cJSON_CreateString(HA_CfgSensors[i].DeviceClass));
        cJSON_AddItemToObject(jsConfig, "state_class", cJSON_CreateString(HA_CfgSensors[i].StateClass));
        cJSON_AddItemToObject(jsConfig, "state_topic", cJSON_CreateString(topicBuf));
        cJSON_AddItemToObject(jsConfig, "unit_of_measurement", cJSON_CreateString(HA_CfgSensors[i].Unit));

        snprintf(haTopic, sizeof(haTopic), "%s/sensor/%s/config", mqtt_user->mqtt_ha_topic, HA_CfgSensors[i].UniqueId);

        char *strJson = cJSON_Print(jsConfig);
        esp_mqtt_client_publish(mqtt_user->mqtt_client, haTopic, strJson, 0, 0, 0);
        free(strJson);

        cJSON_Delete(jsConfig);
    }

    return ESP_OK;
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    MQTT_user_t *mqtt_user = (MQTT_user_t *)handler_args;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "HA Topic: %s", mqtt_user->mqtt_ha_topic);
            HAConfigMessage(mqtt_user);
            break;

        default:
            break;
    }
}
