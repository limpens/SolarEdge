#include <stdio.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_err.h>

#include "TaskModbus.h"
#include "private_types.h"

#define TAG "TaskModbus"

void TaskModbus(void *param)
{
    TaskModbus_t *data = (TaskModbus_t *)param;

    while (data->mb->Connect() != ESP_OK)
    {
        ESP_LOGI(TAG, "Retry connection..");
        vTaskDelay(pdMS_TO_TICKS(2500));
    }

    ESP_LOGI(TAG, "Connected");

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(MODBUS_QUERY_DELAY));

        if (!data->mb->is_connected())
        {
            ESP_LOGI(TAG, "Disconnected");
            data->mb->Connect();
        }

        esp_err_t error = data->mb->ReadRegisters(data->sunspec);

        if (error != ESP_OK)
        {
            ESP_LOGI(TAG, "Read error: %d", error);
            data->mb->Close();

            continue;
        }

        data->mb->ConvertRegisters(data->sunspec, data->solaredge);

        xSemaphoreGive(data->lock);
    }
}
