#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "sunspec.h"
#include "modbus.h"

typedef struct
{
    modbus *mb;
    SemaphoreHandle_t lock;
    SolarEdgeSunSpec_t *sunspec;
    SolarEdge_t *solaredge;
} TaskModbus_t;
