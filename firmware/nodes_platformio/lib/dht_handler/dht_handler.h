#include <Arduino.h>
#include <DHT_U.h>
#include <DHT.h>

#define DHT_DELAY_DEFAULT 2000000L

class DHT_Handler
{
public:
    DHT_Handler(uint8_t pin, uint8_t type);

    uint32_t getDelayUs();
    void setDelayUs(uint32_t delayUs);

    void begin();
    void dhtSensorInfo();

    // Trả TRUE chỉ khi có MẪU MỚI (đúng chu kỳ). Nếu chỉ muốn lấy cache, dùng getters.
    bool readTemperature(float &out_temp);
    bool readHumidity(float &out_hum);
    bool readTemperatureAndHumidity(float &out_temp, float &out_hum);

    // Getters lấy cache (luôn trả về giá trị gần nhất nếu hợp lệ, không đụng timer)
    bool getLastTemperature(float &out_temp) const
    {
        if (!_lastValid || isnan(_lastTemp))
            return false;
        out_temp = _lastTemp;
        return true;
    }
    bool getLastHumidity(float &out_hum) const
    {
        if (!_lastValid || isnan(_lastHum))
            return false;
        out_hum = _lastHum;
        return true;
    }

private:
    bool pollIfDue_(); // đọc CẢ HAI khi đến kỳ, cập nhật cache, trả true nếu vừa đọc mới

    uint8_t _pin;
    uint8_t _type;
    DHT_Unified _dht;
    uint32_t _minDelayUs = DHT_DELAY_DEFAULT;
    uint32_t delayUs = DHT_DELAY_DEFAULT;

    bool _initialized = false;

    // Timer & cache dùng CHUNG cho cả Temp & Hum
    uint32_t _lastReadUs = 0;
    float _lastTemp = NAN;
    float _lastHum = NAN;
    bool _lastValid = false;
};
