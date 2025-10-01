#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dht11_reader.h"

void app_main(void)
{
    xTaskCreate(read_dht_task, "read_dht_task", 2048, NULL, 5, NULL);
}
