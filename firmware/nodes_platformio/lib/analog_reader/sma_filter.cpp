#include "sma_filter.h"

SMAFilter::SMAFilter()
    : _ringBuffer(nullptr), _windowSize(0), _idx(0), _sum(0), _count(0) {}

SMAFilter::SMAFilter(uint8_t windowSize) : _windowSize(windowSize)
{
    _ringBuffer = new uint16_t[windowSize]();
    _idx = 0;
    _count = 0;
    _sum = 0;
}

void SMAFilter::setWindowSize(uint8_t size)
{
    _windowSize = size;
    delete[] _ringBuffer;
    _ringBuffer = new uint16_t[_windowSize]();
    _sum = 0;
    _idx = 0;
    _count = 0;
}

void SMAFilter::reset()
{
    delete[] _ringBuffer;
    _ringBuffer = new uint16_t[_windowSize]();
    _sum = 0;
    _idx = 0;
    _count = 0;
}

bool SMAFilter::ready() const
{
    return _count >= _windowSize;
}

uint16_t SMAFilter::process(uint16_t sample)
{
    if (!this->ready())
    {
        _sum += sample;
        _ringBuffer[_idx] = sample;
        _idx = (_idx + 1) % _windowSize;
        ++_count;
        return sample;
    }

    _sum -= _ringBuffer[_idx];
    _ringBuffer[_idx] = sample;
    _sum += sample;
    _idx = (_idx + 1) % _windowSize;
    return _sum / _windowSize;
}
