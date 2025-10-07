#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_oled_display.h"
#include "esp_log.h"
#include "global.h"
#include "dht11_reader.h"
#include "wifi_connect.h"
#include "lwip/inet.h"

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
    /*========== Connect to Wi-fi ==========*/
    wifi_conn_config_t wifi_cfg = {
        .ssid = NULL, // Use menuconfig configuration
        .password = NULL,
        .max_retry = -1, // Use CONFIG_WIFI_CONN_MAX_RETRY
        .auto_start = true,
    };

    wifi_conn_init(&wifi_cfg);

    if (wifi_conn_wait_ip(15000) == ESP_OK)
    {
        uint32_t ip;
        if (wifi_conn_get_ipv4(&ip))
        {
            struct in_addr a = {.s_addr = ip};
            ESP_LOGI("APP", "Wi-Fi ready, IP: %s", inet_ntoa(a));
        }
    }
    else
    {
        ESP_LOGE("APP", "Wi-Fi connect failed or timeout");
    }

    /*========== Read DHT11 Data ==========*/
    static dht11_t dht11_sensor;
    dht11_sensor = dht11_init(DHT11_PIN);
    xTaskCreate(read_dht11_and_update_globals_task, "dht11_task", 3072, &dht11_sensor, 5, NULL);
    ESP_LOGI(TAG, "DHT11 task created");

    /*========== Display temparature, humidity to I2C OLED Screen ==========*/
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
