#include "dht_handler.h"
#include <Adafruit_Sensor.h>

DHT_Handler::DHT_Handler(uint8_t pin, uint8_t type)
    : _pin(pin), _type(type), _dht(pin, type) {}

void DHT_Handler::begin()
{
    _dht.begin();
    _initialized = true;

    sensor_t t_sensor, h_sensor;
    _dht.temperature().getSensor(&t_sensor);
    _dht.humidity().getSensor(&h_sensor);

    _minDelayUs = (t_sensor.min_delay > h_sensor.min_delay) ? t_sensor.min_delay : h_sensor.min_delay;

    // đảm bảo delayUs không nhỏ hơn min của cảm biến
    if (delayUs < _minDelayUs)
        delayUs = _minDelayUs;

    _lastReadUs = 0;
    _lastValid = false;
    _lastTemp = NAN;
    _lastHum = NAN;
}

uint32_t DHT_Handler::getDelayUs() { return delayUs; }

void DHT_Handler::setDelayUs(uint32_t duration)
{
    // ép không được nhỏ hơn minDelay của sensor
    delayUs = (duration < _minDelayUs) ? _minDelayUs : duration;
}

bool DHT_Handler::pollIfDue_()
{
    if (!_initialized)
        return false;

    const uint32_t now = micros();
    if (now - _lastReadUs < delayUs)
    {
        return false; // chưa đến kỳ, không đụng cache
    }

    _lastReadUs = now;

    sensors_event_t t_event, h_event;
    _dht.temperature().getEvent(&t_event);
    _dht.humidity().getEvent(&h_event);

    if (!isnan(t_event.temperature) && !isnan(h_event.relative_humidity))
    {
        _lastTemp = t_event.temperature;
        _lastHum = h_event.relative_humidity;
        _lastValid = true;
        return true; // VỪA có mẫu MỚI
    }

    // Lỗi: giữ cache cũ, báo không có mẫu MỚI
    return false;
}

// ===== API =====

// Ba hàm dưới đây CHỈ trả true khi có MẪU MỚI (đúng chu kỳ).
// Nếu bạn muốn lấy lại giá trị gần nhất dù chưa tới kỳ, dùng getLastTemperature()/getLastHumidity().

bool DHT_Handler::readTemperature(float &out_temp)
{
    if (!_initialized)
        return false;
    const bool newSample = pollIfDue_();
    if (!newSample)
        return false; // chỉ báo khi vừa đọc mới
    if (!_lastValid || isnan(_lastTemp))
        return false;
    out_temp = _lastTemp;
    return true;
}

bool DHT_Handler::readHumidity(float &out_hum)
{
    if (!_initialized)
        return false;
    const bool newSample = pollIfDue_();
    if (!newSample)
        return false; // chỉ báo khi vừa đọc mới
    if (!_lastValid || isnan(_lastHum))
        return false;
    out_hum = _lastHum;
    return true;
}

bool DHT_Handler::readTemperatureAndHumidity(float &out_temp, float &out_hum)
{
    if (!_initialized)
        return false;
    const bool newSample = pollIfDue_();
    if (!newSample)
        return false; // chỉ báo khi vừa đọc mới
    if (!_lastValid || isnan(_lastTemp) || isnan(_lastHum))
        return false;
    out_temp = _lastTemp;
    out_hum = _lastHum;
    return true;
}
