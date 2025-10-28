#pragma once

#include <stdint.h>

class SMAFilter
{

public:
    explicit SMAFilter();
    explicit SMAFilter(uint8_t windowSize);
    void setWindowSize(uint8_t windowSize);
    void reset();
    uint16_t process(uint16_t sample);
    bool ready() const;

private:
    uint16_t *_ringBuffer;
    uint8_t _windowSize, _idx, _count;
    uint32_t _sum;
};