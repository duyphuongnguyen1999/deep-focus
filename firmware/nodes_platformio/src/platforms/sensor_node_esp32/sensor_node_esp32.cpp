#include "sensor_node_esp32.h"
#include "dht_handler.h"
#include "analog_reader.h"
#include <driver/adc.h>

DHT_Handler dht(DHT_PIN, DHT11);
AnalogReader ar(MH_ANALOG_PIN);

void setupSensorNode()
{
    Serial.begin(115200);
    dht.begin();
    // analogSetPinAttenuation(MH_ANALOG_PIN, ADC_11db);
}

void loopSensorNode()
{
    // Main loop logic for the sensor node
    float out_temp, out_hum;
    dht.readTemperature(out_temp);
    dht.readHumidity(out_hum);
    Serial.print("Light Analog Signal: ");
    Serial.println(ar.readSmoothed());
}