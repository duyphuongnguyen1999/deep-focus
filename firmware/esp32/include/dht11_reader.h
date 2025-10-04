#ifndef DHT11_READER_H
#define DHT11_READER_H

#include <stdint.h>

#include "driver/gpio.h"
#include "dht.h"
#include "esp_err.h"

typedef struct
{
    dht_sensor_type_t type; // Sensor type (DHT11 or DHT22)
    gpio_num_t pin;         // GPIO pin connected to DHT11
    int16_t temperature;    // Temperature in tenths of degrees Celsius
    int16_t humidity;       // Humidity in tenths of percent
} dht11_t;

dht11_t dht11_init(gpio_num_t pin);
esp_err_t dht11_read(dht11_t *sensor);
void read_dht11_task(void *pvParameter);
void read_dht11_and_update_globals_task(void *pvParameter);

#endif // DHT11_READER_H