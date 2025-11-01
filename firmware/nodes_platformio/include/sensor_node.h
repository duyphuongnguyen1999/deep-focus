#pragma once
#include <Arduino.h>

#ifdef ESP32
#define DHT_PIN 4
#define MH_ANALOG_PIN 34
#elif defined(ESP8266) || defined(ESP8266_NODE_MCU)
#define DHT_PIN 14 // D5
#define MH_ANALOG_PIN A0
#endif
