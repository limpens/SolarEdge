#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"


#include "TaskModbus.h"

#define TAG "TaskModbus"

void TaskModbus(void *param)
{
TaskModus_t *data = (TaskModus_t*)param;

  //data->mb->SetSlaveID(1);

  while (data->mb->Connect() != ESP_OK)
  {
    ESP_LOGI(TAG, "Retry connection..");
    vTaskDelay(pdMS_TO_TICKS(2500));
  }

  ESP_LOGI(TAG, "Connected");

  while(true)
  {
    // according to sunspec-implementation-technical-note.pdf Appendix B,
    // the update frequency can be 1 second, 
    vTaskDelay(pdMS_TO_TICKS(MODBUS_QUERY_DELAY));

    if (!data->mb->is_connected())
    {
      ESP_LOGI(TAG, "Disconnected");
      data->mb->Connect();
    }

    // query all registers in one packet:
    esp_err_t error = data->mb->ReadRegisters(data->sunspec);

    if (error != ESP_OK)
    {
      ESP_LOGI(TAG, "Read error: %d", error);
      data->mb->Close();

      continue;
    }

    data->mb->ConvertRegisters(data->sunspec, data->solaredge);

    // signal app_main about the new data:
    xSemaphoreGive(data->lock);
  } // while

}