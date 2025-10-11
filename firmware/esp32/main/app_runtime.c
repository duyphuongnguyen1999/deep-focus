#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lwip/inet.h"

#include "app_config.h"
#include "app_runtime.h"

#include "wifi_connect.h"
#include "dht11_reader.h"
#include "i2c_oled_display.h"
#include "global.h"

static const char *TAG = "APP_RUNTIME";

/*========== LVGL ========== */
// Handle display for "update_task"
static lv_display_t *s_disp = NULL;

// Update OLED screen every second
static void updater_task(void *arg)
{
    (void)arg;
    while (1)
    {
        float temp_c = (float)(global_temperature) / 10.0f;
        float hum_pct = (float)(global_humidity) / 10.0f;

        // Update UI
        oled_display_update(temp_c, hum_pct);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*========== Wi-fi Connection ========== */
static esp_err_t start_wifi(void)
{
    wifi_conn_config_t wifi_cfg = {
        .ssid = NULL, // Use menuconfig configuration
        .password = NULL,
        .max_retry = -1, // Use CONFIG_WIFI_CONN_MAX_RETRY
        .auto_start = true,
    };

    ESP_RETURN_ON_ERROR(wifi_conn_init(&wifi_cfg), TAG, "wifi_conn_init failed");

    if (wifi_conn_wait_ip(15000) == ESP_OK)
    {
        uint32_t ip;
        if (wifi_conn_get_ipv4(&ip))
        {
            struct in_addr a = {.s_addr = ip};
            ESP_LOGI(TAG, "Wi-Fi ready, IP: %s", inet_ntoa(a));
        }
        else
        {
            ESP_LOGW(TAG, "Got IP but wifi_conn_get_ipv4() failed");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Wi-Fi connect failed or timeout");
        // Not return ESP_FAIl so that the program could run offline mode
        // return ESP_FAIL;
    }
    return ESP_OK;
}

/*========== Read DHT11 Data ==========*/
static esp_err_t start_dht11_task(void)
{
    static dht11_t dht11_sensor;
    dht11_sensor = dht11_init(DHT11_PIN);

    BaseType_t ok = xTaskCreate(
        read_dht11_and_update_globals_task,
        "dht11_task",
        3072,
        &dht11_sensor,
        5,
        NULL);
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "Create dht11_task failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "DHT11 task created");
    return ESP_OK;
}

/*========== OLED Initialization ==========*/
static esp_err_t init_oled_and_ui(void)
{
    s_disp = oled_display_init(I2C_PORT, SDA_IO, SCL_IO, I2C_ADDR, OLED_W, OLED_H);
    if (!s_disp)
    {
        ESP_LOGE(TAG, "Failed to initialize OLED display");
        return ESP_FAIL;
    }
    oled_display_create_basic_ui(s_disp);
    ESP_LOGI(TAG, "OLED display initialized");
    return ESP_OK;
}

// Public APIs
esp_err_t app_runtime_init(void)
{
    // 1. Wi-Fi (still ok in offline mode)
    ESP_ERROR_CHECK_WITHOUT_ABORT(start_wifi());

    // 2. DHT11
    ESP_RETURN_ON_ERROR(start_dht11_task(), TAG, "start_dht11_task failed");

    // 3. OLED + UI
    ESP_RETURN_ON_ERROR(init_oled_and_ui(), TAG, "init_oled_and_ui failed");

    return ESP_OK;
}

esp_err_t app_runtime_start(void)
{
    // Create task update UI
    BaseType_t ok = xTaskCreate(
        updater_task,
        "ui_update",
        4096,
        NULL,
        3,
        NULL);
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "Create ui_update task failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "UI update task created");
    return ESP_OK;
}
