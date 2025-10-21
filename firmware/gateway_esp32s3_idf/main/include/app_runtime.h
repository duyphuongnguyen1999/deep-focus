#pragma once
#include "esp_err.h"

// Initialize app components (Wi-fi, sensor, display,...)
esp_err_t app_runtime_init(void);

// Start tasks
esp_err_t app_runtime_start(void);
