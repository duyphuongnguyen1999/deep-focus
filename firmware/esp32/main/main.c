#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "dht.h"

#define TAG "DHT11_DEMO"

#define DHT_TYPE DHT_TYPE_DHT11
#define DHT_GPIO GPIO_NUM_4

void read_dht_task(void *pvParameter)
{
    ESP_LOGI(TAG, "DHT11 task started on GPIO %d", (int)DHT_GPIO);
    while (1)
    {
        int16_t temperature, humidity;
        if (dht_read_data(DHT_TYPE, DHT_GPIO, &humidity, &temperature) == ESP_OK)
        {
            ESP_LOGI(TAG, "Humidity: %.1f%%, Temperature: %.1f°C", (float)humidity / 10, (float)temperature / 10);
        }
        else
        {
            ESP_LOGW(TAG, "Could not read data from sensor");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    xTaskCreate(read_dht_task, "read_dht_task", 2048, NULL, 5, NULL);
}
