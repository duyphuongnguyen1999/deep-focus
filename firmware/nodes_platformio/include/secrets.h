#pragma once
#include <Arduino.h>
#define BLYNK_TEMPLATE_ID "TMPL6kkzXdQiV"
#define BLYNK_TEMPLATE_NAME "DeepFocus"

#if defined(SENSOR_NODE_ESP8266)
#define BLYNK_AUTH_TOKEN "rsHF2X8Kd-cU9WzIddqAvRVXia8wBDCi"
#elif defined(CONTROLLER_NODE_ESP32)
#define BLYNK_AUTH_TOKEN "iwi6t1h1RLwbJcLhtKBrTvR0-o3hAqOu"
#endif

// WiFi Credentials
const char ssid[] = "Duy Phuong (tang 3)";
const char pass[] = "Phuong1234";