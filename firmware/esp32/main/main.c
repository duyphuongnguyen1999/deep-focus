#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_oled_display.h"
#include "esp_log.h"
#include "global.h"
#include "dht11_reader.h"

// DHT11 configuration
#define DHT11_PIN GPIO_NUM_4

// I2C + OLED configuration
#define I2C_PORT 0
#define SDA_IO 21
#define SCL_IO 22
#define I2C_ADDR 0x3C
#define OLED_W 128
#define OLED_H 64

static const char *TAG = "APP";

static void updater_task(void *arg)
{
    while (1)
    {
        float temp_c = (float)(global_temperature) / 10.0f;
        float hum_pct = (float)(global_humidity) / 10.0f;
        oled_display_update(temp_c, hum_pct);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    static dht11_t dht11_sensor;
    dht11_sensor = dht11_init(DHT11_PIN);
    xTaskCreate(read_dht11_and_update_globals_task, "dht11_task", 3072, &dht11_sensor, 5, NULL);
    ESP_LOGI(TAG, "DHT11 task created");

    // Initialize OLED display
    lv_display_t *disp = oled_display_init(I2C_PORT, SDA_IO, SCL_IO, I2C_ADDR, OLED_W, OLED_H);
    if (!disp)
    {
        ESP_LOGE(TAG, "Failed to initialize OLED display");
        return;
    }
    // Initialize UI
    oled_display_create_basic_ui(disp);
    ESP_LOGI(TAG, "OLED display initialized");
    // Update display task
    xTaskCreate(updater_task, "ui_update", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "UI update task created");
}
