#pragma once

#include <esp_err.h>
#include <mqtt_client.h>

#include "private_types.h"

typedef struct
{
    const char *mqtt_ha_topic;
    const char *mqtt_topic;
    esp_mqtt_client_handle_t mqtt_client;
} MQTT_user_t;

typedef struct
{
    const char *Name;
    const char *UniqueId;
    const char *DeviceClass;
    const char *StateClass;
    const char *Unit;
} HA_ConfigMsg_T;

esp_err_t PublishMQTT(MQTT_user_t *user, TaskModbus_t *data);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
