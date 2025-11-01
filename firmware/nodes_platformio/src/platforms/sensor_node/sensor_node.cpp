#include "sensor_node.h"
#include "dht_handler.h"
#include "analog_reader.h"
#include "secrets.h"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// Cache (dùng float cho các đại lượng so sánh %)
float lastTemp = NAN;
float lastHum = NAN;
float lastLightPct = NAN; // dùng cái này cho deadband
int lastLightRaw = -1;    // chỉ để debug / gửi kèm, không dùng so sánh %

unsigned long lastSendMs = 0;
unsigned long lastReadMs = 0;

// Send data config
const float DEAD_BAND_PERCENT = 0.05f;     // 5%
const unsigned long T_MAX_MS = 180000;     // 90s
const unsigned long READ_PERIOD_MS = 2500; // DHT >= 2s/lần

BlynkTimer timer;

DHT_Handler dht(DHT_PIN, DHT11);
AnalogReader ar(MH_ANALOG_PIN);

bool changedOverPct(float cur, float prev, float pct)
{
    if (isnan(cur))
        return false; // giá trị hiện tại không hợp lệ => không gửi
    if (isnan(prev))
        return true; // lần đầu
    float denom = fabs(prev);
    if (denom < 1e-6f)
        return fabs(cur - prev) > 1e-6f; // tránh chia 0
    float rel = fabs((cur - prev) / denom);
    return rel >= pct;
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    dht.begin();
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    lastSendMs = 0; // để lần đầu (sau khi đọc OK) có thể gửi ngay
    lastReadMs = millis();
}

void loop()
{
    Blynk.run();

    unsigned long now = millis();
    if (now - lastReadMs < READ_PERIOD_MS)
        return; // tôn trọng chu kỳ DHT
    lastReadMs = now;

    // Đọc cảm biến
    float temp = NAN, hum = NAN, lightPct = NAN;
    int lightRaw = -1;

    if (!dht.readTemperatureAndHumidity(temp, hum))
    {
        // Serial.println("DHT read failed, skip this cycle");
        return; // không có dữ liệu hợp lệ thì bỏ lượt
    }

    // Clamp độ ẩm cho chắc
    if (hum < 0)
        hum = 0;
    if (hum > 100)
        hum = 100;

    lightRaw = ar.readSmoothed();                   // 0..1023
    lightPct = (1 - (lightRaw / 1023.0f)) * 100.0f; // % sáng tương đối

    // Quyết định gửi theo deadband 5% hoặc T_MAX
    bool needSend = false;
    if (changedOverPct(temp, lastTemp, DEAD_BAND_PERCENT))
    {
        needSend = true;
        Blynk.virtualWrite(V2, temp);
        lastTemp = temp;
    }
    if (changedOverPct(hum, lastHum, DEAD_BAND_PERCENT))
    {
        needSend = true;
        Blynk.virtualWrite(V3, hum);
        lastHum = hum;
    }
    if (changedOverPct(lightPct, lastLightPct, DEAD_BAND_PERCENT))
    {
        needSend = true;
        Blynk.virtualWrite(V4, lightPct);
        lastLightPct = lightPct;
        lastLightRaw = lightRaw;
    }
    if (now - lastSendMs >= T_MAX_MS)
    {
        needSend = true;
        Blynk.virtualWrite(V2, temp);
        Blynk.virtualWrite(V3, hum);
        Blynk.virtualWrite(V4, lightPct);
        lastTemp = temp;
        lastHum = hum;
        lastLightPct = lightPct;
        lastLightRaw = lightRaw;
        lastSendMs = now;
    }

    // Logging
    if (needSend)
    {
        String payload = String("temp=") + String(lastTemp, 1) +
                         ",hum=" + String(lastHum, 1) +
                         ",light=" + String(lastLightRaw) +
                         ",lightPct=" + String(lastLightPct, 0);
        Serial.println(payload);
    }
}
