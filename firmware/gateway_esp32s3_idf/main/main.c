#include "esp_err.h"
#include "esp_system.h"
#include "app_runtime.h"
#include "uart_bridge.h"

void app_main(void)
{
    if (app_runtime_init() != ESP_OK)
    {
        // Stop when init fail
        // esp_restart();
    }

    if (app_runtime_start() != ESP_OK)
    {
        // Stop when start fail
        // esp_restart();
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_bridge_start(2000)); // print JSON every 2s
}
