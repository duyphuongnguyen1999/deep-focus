#include <Arduino.h>
#include "dht_handler.h"

DHT_Handler dht(4, DHT11);

void setupSensorNode()
{
    Serial.begin(115200);
    dht.begin();
}

void loopSensorNode()
{
    // Main loop logic for the sensor node
    float out_temp, out_hum;
    dht.readTemperature(out_temp);
    dht.readHumidity(out_hum);
}