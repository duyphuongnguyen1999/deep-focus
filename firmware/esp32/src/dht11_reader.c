#include "dht11_reader.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "dht.h"
#include "dht11_reader.h"

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