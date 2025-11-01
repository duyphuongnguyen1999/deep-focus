#pragma once
#include <Arduino.h>
#include "sma_filter.h"

class AnalogReader
{
public:
    enum class FilterMethod
    {
        SMA, // Simple Moving Average
        EMA, // TODO Exponential Moving Average : Implements later
        WMA, // TODO Weighted Moving Average : Implements later
        NONE
    };
    // Default config
    static constexpr uint16_t kDefaultMinAnalogValue = 0;
#ifdef ESP32
    static constexpr uint16_t kDefaultMaxAnalogValue = 4095;
    static constexpr float kDefaultVref = 3.3f;
#elif defined(ESP8266)
    static constexpr uint16_t kDefaultMaxAnalogValue = 1023;
    static constexpr float kDefaultVref = 1.0f;
#elif defined(ESP8266_NODE_MCU)
    static constexpr uint16_t kDefaultMaxAnalogValue = 1023;
    static constexpr float kDefaultVref = 3.3f;
#endif
    static constexpr FilterMethod kDefaultFilterMethod = FilterMethod::SMA;
    static constexpr uint32_t kDefaultPeriodUs = 200000; // 200ms
    static constexpr uint8_t kDefaultWindowSize = 10;
    static constexpr float kDefaultSmoothingFactor = 0.2f;

    explicit AnalogReader(
        uint8_t pin,
        uint16_t minAnalogValue = kDefaultMinAnalogValue,
        uint16_t maxAnalogValue = kDefaultMaxAnalogValue,
        FilterMethod method = kDefaultFilterMethod,
        uint32_t periodUs = kDefaultPeriodUs);

    // Setters / Getters
    void setFilterMethod(FilterMethod m);
    void setWindowSize(uint8_t ws);
    void setPeriodUs(uint32_t period);
    void setSmoothingFactor(float factor);
    void setAutoCal(bool autoCal);

    uint16_t readRaw();
    uint16_t readSmoothed();
    float readNormalized(); // Min-Max Normalizer
    float readNormalized(float upper, float lower);
    float readVoltage(float vref = 3.3f);

private:
    uint8_t _pin;
    uint16_t _minAnalogValue, _maxAnalogValue;
    FilterMethod _method;
    uint8_t _windowSize;
    float _smoothingFactor;
    uint32_t _periodUs, _lastReadUs = 0;
    uint16_t _lastReadValue = 0;
#ifdef ESP32
    uint16_t _runMin = 4095;
#elif defined(ESP8266) || defined(ESP8266_NODE_MCU)
    uint16_t _runMin = 1023;
#endif
    uint16_t _runMax = 0;
    bool _autoCal = false;

    // SMA Filter
    SMAFilter _smaFilter;
};