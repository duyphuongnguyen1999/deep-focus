#pragma once
#include <Arduino.h>

// MAC address of the ESP32-S3 Gateway
static const uint8_t ESP32_S3_GATEWAY_MAC[6] = {0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF};

// WiFi Credentials
static const char *GATEWAY_SSID = "YourWiFiSSID";
static const char *GATEWAY_PASSWORD = "YourWiFiPassword";
static const unint8_t WIFI_CHANNEL = 1;