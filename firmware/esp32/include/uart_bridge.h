#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Start uart_bridge_task, print JSON line to stdout (UART) every interval_ms
    esp_err_t uart_bridge_start(int interval_ms);

#ifdef __cplusplus
}
#endif
