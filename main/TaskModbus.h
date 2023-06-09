#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "modbus.h"
#include "sunspec.h"

//
// number of milliseconds between requesting new data
//
#define MODBUS_QUERY_DELAY (1000)

void TaskModbus(void *param);

typedef struct
{
  modbus *mb;                   // modbus class, initialized on the heap in the app_main
  SemaphoreHandle_t lock;       // semaphore to signal new data
  SolarEdgeSunSpec_t *sunspec;  // raw data from the SolarEdge inverter
  SolarEdge_t *solaredge;       // parsed data, ready for user interface
}TaskModus_t;