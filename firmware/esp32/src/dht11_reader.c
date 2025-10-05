#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "dht11_reader.h"
#include "global.h"

static const char *TAG = "DHT11_READER";

dht11_t dht11_init(gpio_num_t pin)
{
    dht11_t sensor = {
        .type = DHT_TYPE_DHT11,
        .pin = pin,
        .temperature = 0x7FFF, // Uninitialized state
        .humidity = 0x7FFF     // Uninitialized state
    };

    // Configure the GPIO pin
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    ESP_LOGI(TAG, "DHT11 sensor initialized on GPIO %d", pin);
    return sensor;
}

esp_err_t dht11_read(dht11_t *sensor)
{
    return dht_read_data(sensor->type, sensor->pin, &sensor->humidity, &sensor->temperature);
}

void read_dht11_task(void *pvParameter)
{
    dht11_t *sensor = (dht11_t *)pvParameter;
    while (1)
    {
        if (dht11_read(sensor) == ESP_OK)
        {
            ESP_LOGI(TAG, "Humidity: %.1f%%, Temperature: %.1f°C",
                     (float)sensor->humidity / 10, (float)sensor->temperature / 10);
        }
        else
        {
            ESP_LOGW(TAG, "Could not read data from sensor");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void read_dht11_and_update_globals_task(void *pvParameter)
{
    dht11_t *sensor = (dht11_t *)pvParameter;
    while (1)
    {
        if (dht11_read(sensor) == ESP_OK)
        {
            ESP_LOGI(TAG, "Humidity: %.1f%%, Temperature: %.1f°C",
                     (float)sensor->humidity / 10, (float)sensor->temperature / 10);
            global_humidity = sensor->humidity;
            global_temperature = sensor->temperature;
        }
        else
        {
            ESP_LOGW(TAG, "Could not read data from sensor");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
