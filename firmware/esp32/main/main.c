#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dht11_reader.h"

void app_main(void)
{
    static dht11_t dht11_sensor;
    dht11_sensor = dht11_init(GPIO_NUM_4);
    xTaskCreate(read_dht11_and_update_globals_task,
                "dht11_task",
                3072,
                &dht11_sensor,
                5,
                NULL);
}
