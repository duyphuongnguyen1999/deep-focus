#include "dht_handler.h"
#include <Adafruit_Sensor.h>

DHT_Handler::DHT_Handler(uint8_t pin, uint8_t type)
    : _pin(pin), _type(type), _dht(pin, type)
{
}

void DHT_Handler::begin()
{
    _dht.begin();
    _initialized = true;
    sensor_t t_sensor, h_sensor;
    _dht.temperature().getSensor(&t_sensor);
    _dht.humidity().getSensor(&h_sensor);
    _minDelayUs = (t_sensor.min_delay > h_sensor.min_delay) ? t_sensor.min_delay : h_sensor.min_delay;
    delayUs = _minDelayUs;
}

uint32_t DHT_Handler::getDelayUs()
{
    return delayUs;
}

void DHT_Handler::setDelayUs(uint32_t duration)
{
    delayUs = duration;
}

void DHT_Handler::dhtSensorInfo()
{
    // Initialize device.
    if (!_initialized)
    {
        Serial.println("DHT not initialized. Call begin() first.");
        return;
    }
    // Print temperature sensor details.
    sensor_t sensor;
    _dht.temperature().getSensor(&sensor);
    Serial.println(F("------------------------------------"));
    Serial.println(F("Temperature Sensor"));
    Serial.print(F("Sensor Type: "));
    Serial.println(sensor.name);
    Serial.print(F("Driver Ver:  "));
    Serial.println(sensor.version);
    Serial.print(F("Unique ID:   "));
    Serial.println(sensor.sensor_id);
    Serial.print(F("Max Value:   "));
    Serial.print(sensor.max_value);
    Serial.println(F("°C"));
    Serial.print(F("Min Value:   "));
    Serial.print(sensor.min_value);
    Serial.println(F("°C"));
    Serial.print(F("Resolution:  "));
    Serial.print(sensor.resolution);
    Serial.println(F("°C"));
    Serial.println(F("------------------------------------"));

    // Print humidity sensor details.
    _dht.humidity().getSensor(&sensor);
    Serial.println(F("Humidity Sensor"));
    Serial.print(F("Sensor Type: "));
    Serial.println(sensor.name);
    Serial.print(F("Driver Ver:  "));
    Serial.println(sensor.version);
    Serial.print(F("Unique ID:   "));
    Serial.println(sensor.sensor_id);
    Serial.print(F("Max Value:   "));
    Serial.print(sensor.max_value);
    Serial.println(F("%"));
    Serial.print(F("Min Value:   "));
    Serial.print(sensor.min_value);
    Serial.println(F("%"));
    Serial.print(F("Resolution:  "));
    Serial.print(sensor.resolution);
    Serial.println(F("%"));
    Serial.println(F("------------------------------------"));
}

bool DHT_Handler::readTemperature(float &out_temp)
{
    if (!_initialized)
    {
        return false;
    }

    sensors_event_t event;

    _dht.temperature().getEvent(&event);
    if (isnan(event.temperature))
    {
        return false;
    }

    out_temp = event.temperature;

    delayMicroseconds(delayUs);
    Serial.print("Temperature: ");
    Serial.println(out_temp);
    return true;
}

bool DHT_Handler::readHumidity(float &out_hum)
{
    if (!_initialized)
    {
        return false;
    }

    sensors_event_t event;

    _dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity))
    {
        return false;
    }

    out_hum = event.relative_humidity;

    delayMicroseconds(delayUs);
    Serial.print("Humidity: ");
    Serial.println(out_hum);
    return true;
}