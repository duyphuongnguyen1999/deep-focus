#include "analog_reader.h"

AnalogReader::AnalogReader(
    uint8_t pin,
    uint16_t minAnalogValue,
    uint16_t maxAnalogValue,
    FilterMethod method,
    uint32_t periodUs)
    : _pin(pin),
      _minAnalogValue(minAnalogValue),
      _maxAnalogValue(maxAnalogValue),
      _method(method),
      _periodUs(periodUs),
      _lastReadValue(0),
      _windowSize(kDefaultWindowSize), // default
      _smoothingFactor(kDefaultSmoothingFactor)
{
    // Ensure min < max
    if (_minAnalogValue >= _maxAnalogValue)
    {
        _minAnalogValue = kDefaultMinAnalogValue;
        _maxAnalogValue = kDefaultMaxAnalogValue;
    }
    switch (_method)
    {
    case FilterMethod::SMA:
        _windowSize = kDefaultWindowSize;
        _smaFilter.setWindowSize(_windowSize);
        break;

    case FilterMethod::EMA: // TODO Implement later
    case FilterMethod::WMA: // TODO Implement later
    case FilterMethod::NONE:
    default:
        _windowSize = 1;
        break;
    }
}

void AnalogReader::setFilterMethod(FilterMethod m)
{
    _method = m;
}
void AnalogReader::setWindowSize(uint8_t ws)
{
    _windowSize = ws;
}
void AnalogReader::setPeriodUs(uint32_t p)
{
    _periodUs = p;
}
void AnalogReader::setSmoothingFactor(float factor)
{
    _smoothingFactor = factor;
}
void AnalogReader::setAutoCal(bool autoCal)
{
    _autoCal = autoCal;
}

uint16_t AnalogReader::readRaw()
{
    const uint32_t now = micros();
    if (now - _lastReadUs < _periodUs)
        return _lastReadValue;
    _lastReadUs = now;
    _lastReadValue = analogRead(_pin);
    return _lastReadValue;
}

uint16_t AnalogReader::readSmoothed()
{
    const uint16_t sample = AnalogReader::readRaw();
    switch (_method)
    {
    case FilterMethod::SMA:
        return _smaFilter.process(sample);

    case FilterMethod::EMA: // TODO Implement later
    case FilterMethod::WMA: // TODO Implement later
    case FilterMethod::NONE:
    default:
        return sample;
    }
}

float AnalogReader::readNormalized()
{
    const uint16_t sample = AnalogReader::readRaw();
    float result;
    if (_autoCal)
    {
        if (sample > _runMax)
            _runMax = sample;
        if (sample < _runMin)
            _runMin = sample;
        if (_runMax == _runMin)
            return 0.0f;
        result = static_cast<float>(sample - _runMin) / static_cast<float>(_runMax - _runMin);
    }
    else
        result = static_cast<float>(sample - _minAnalogValue) / static_cast<float>(_maxAnalogValue - _minAnalogValue);
    return result;
}

float AnalogReader::readNormalized(float upper, float lower)
{
    const float maxV = (upper > lower) ? upper : lower;
    const float minV = (upper > lower) ? lower : upper;

    if (maxV <= minV)
        return 0.0f;

    const uint16_t sample = AnalogReader::readRaw();

    float result = (static_cast<float>(sample) - minV) /
                   (maxV - minV);

    return result;
}

float AnalogReader::readVoltage(float vref)
{
    const uint16_t adc = AnalogReader::readRaw();
    return (static_cast<float>(adc) * vref) / static_cast<float>(kDefaultMaxAnalogValue);
}