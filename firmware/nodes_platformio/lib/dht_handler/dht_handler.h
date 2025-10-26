#include <Arduino.h>
#include <DHT_U.h>
#include <DHT.h>

#define DHT_DELAY_DEFAULT 2000000L

class DHT_Handler
{
private:
    uint8_t _pin;
    uint8_t _type;
    DHT_Unified _dht;
    bool _initialized = false;
    uint32_t _minDelayUs;
    uint32_t delayUs = DHT_DELAY_DEFAULT;

public:
    // Constructor
    DHT_Handler(uint8_t pin, uint8_t type);

    // Getter, setter
    uint32_t getDelayUs();
    void setDelayUs(uint32_t delayUs);

    // Public method
    void begin();
    void dhtSensorInfo();
    bool readTemperature(float &out_temp);
    bool readHumidity(float &out_hum);
};
