#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

#include "modbus.h"
#include "sunspec.h"

//
// number of milliseconds between requesting new data.
// According to sunspec-implementation-technical-note.pdf Appendix B, the update frequency can be 1 second.
//
#define MODBUS_QUERY_DELAY (1000)

void TaskModbus(void *param);
