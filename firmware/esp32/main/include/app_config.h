#pragma once
#include "driver/gpio.h"

// ===== DHT11 =====
#define DHT11_PIN GPIO_NUM_4

// ===== I2C + OLED =====
#define I2C_PORT 0
#define SDA_IO 21
#define SCL_IO 22
#define I2C_ADDR 0x3C
#define OLED_W 128
#define OLED_H 64
