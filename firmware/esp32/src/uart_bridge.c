#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "global.h" // for global_temperature, global_humidity

static const char *TAG = "UART_BRIDGE";

static void uart_bridge_task(void *arg)
{
    const int interval_ms = (int)(intptr_t)arg;
    ESP_LOGI(TAG, "UART bridge started, interval=%dms", interval_ms);

    while (1)
    {
        // Read from global.h
        const float temp_c = (float)global_temperature / 10.0f;
        const float hum_pct = (float)global_humidity / 10.0f;

        // Print JSON line, PC will add system timestamp automatically
        // {"device_id":"esp32_1","temp_c":28.0,"humidity":80.0}
        printf("{\"device_id\":\"esp32_1\",\"temp_c\":%.1f,\"humidity\":%.1f}\n", temp_c, hum_pct);
        fflush(stdout);

        vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }
}

esp_err_t uart_bridge_start(int interval_ms)
{
    if (interval_ms < 100)
        interval_ms = 100;
    BaseType_t ok = xTaskCreate(
        uart_bridge_task,
        "uart_bridge",
        3072,
        (void *)(intptr_t)interval_ms,
        4,
        NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_FAIL;
}
