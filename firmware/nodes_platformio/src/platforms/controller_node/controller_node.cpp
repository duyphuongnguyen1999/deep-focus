// src/platforms/controller_node/controller_node.cpp
// All-in-one: Blynk (V5/V6/V7) + IR send (absolute per temperature) + IR recorder (via Serial)
// Temperature range: 16.0 .. 32.0 °C (step 0.5)
// Files in SPIFFS:
//   /ac/POWER_ON.json
//   /ac/POWER_OFF.json
//   /ac/COOL_<temp>.json (e.g., /ac/COOL_24.5.json)

#include "secrets.h" // BLYNK_TEMPLATE_ID, BLYNK_TEMPLATE_NAME, BLYNK_AUTH_TOKEN, ssid, pass
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

#include <Arduino.h>
#include "SPIFFS.h"

#include <ArduinoJson.h> // ^7.0.4
#include <vector>
#include <cmath>

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h> // resultToHumanReadableBasic(), resultToSourceCode()

// -------------------- Pins & PWM --------------------
static const int LED_PIN = 25; // change to 2 if onboard LED
static const int PWM_CH = 0;
static const int PWM_FREQ = 5000; // 5 kHz
static const int PWM_RES = 8;     // 8-bit (0..255)

static const uint16_t IR_RECV_PIN = 13; // KY-022 OUT
static const uint16_t IR_SEND_PIN = 14; // KY-005 SIG

// -------------------- IR RX config ------------------
const uint16_t kCaptureBufferSize = 1024; // needs build_flags: -D RAW_BUFFER_LENGTH=1024
const uint16_t kIrTimeoutMs = 300;        // end-of-frame timeout (ms)
const bool kUseModulation = true;         // pass-through demodulated receiver? keep true

// Fallback: raw tick duration (typ. 50us per tick)
#ifndef kUsecPerTick
#define kUsecPerTick 50U
#endif

// -------------------- IR objects --------------------
IRsend irsend(IR_SEND_PIN);
IRrecv irrecv(IR_RECV_PIN, kCaptureBufferSize, kIrTimeoutMs, kUseModulation);
decode_results g_results;

// -------------------- Blynk virtual pins ------------
static const uint8_t VPIN_BRIGHTNESS = V5; // 0..255
static const uint8_t VPIN_AC_TEMP = V6;    // absolute target temperature
static const uint8_t VPIN_AC_POWER = V7;   // 0/1

// -------------------- State -------------------------
String g_mode = "COOL"; // extend later if needed
float g_minTemp = 16.0f;
float g_maxTemp = 32.0f;
float g_targetTemp = 24.0f; // last target temp (0.5 step)
int g_brightness = 0;       // 0..255
bool g_overwrite = true;    // overwrite learned files?
bool g_isPoweredOn = false; // estimated AC power state
bool g_autoPowerOn = true;  // if temp set while OFF, auto send POWER_ON first

// Optional debug flood (print every received frame in loop)
bool g_debugIr = false;

// ===================================================
// Helpers
// ===================================================
static inline float quantizeHalf(float t)
{
    t = constrain(t, g_minTemp, g_maxTemp);
    return roundf(t * 2.0f) / 2.0f;
}

static inline String buildAcStatePath(const String &mode, float temp)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", temp);
    return "/ac/" + mode + "_" + String(buf) + ".json";
}

static inline bool loadIrFile(const char *path, std::vector<uint16_t> &outRaw, uint16_t &outFreq)
{
    if (!SPIFFS.exists(path))
    {
        Serial.printf("[IR] File not found: %s\n", path);
        return false;
    }
    File f = SPIFFS.open(path, FILE_READ);
    if (!f)
    {
        Serial.printf("[IR] Cannot open: %s\n", path);
        return false;
    }

    JsonDocument doc; // v7 default construct
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err)
    {
        Serial.printf("[IR] JSON parse error in %s: %s\n", path, err.c_str());
        return false;
    }

    outFreq = doc["frequency"] | 38000;
    JsonArray arr = doc["raw"].as<JsonArray>();
    outRaw.clear();
    outRaw.reserve(arr.size());
    for (JsonVariant v : arr)
    {
        uint32_t us = v.as<uint32_t>();
        if (us > 65535)
            us = 65535;
        outRaw.push_back((uint16_t)us);
    }
    return true;
}

static inline bool sendIrByFile(const char *path)
{
    std::vector<uint16_t> raw;
    uint16_t freq = 38000;
    if (!loadIrFile(path, raw, freq))
        return false;
    Serial.printf("[IR] Sending %s (len=%u, freq=%u Hz)\n", path, (unsigned)raw.size(), freq);
    irsend.sendRaw(raw.data(), raw.size(), freq);
    return true;
}

static inline bool sendAcState(const String &mode, float temp)
{
    String path = buildAcStatePath(mode, temp);
    return sendIrByFile(path.c_str());
}

static inline bool acPowerOn() { return sendIrByFile("/ac/POWER_ON.json"); }
static inline bool acPowerOff() { return sendIrByFile("/ac/POWER_OFF.json"); }

// ===================================================
// Recorder (learn once to path) with debounce
// ===================================================
static inline bool saveJson(const char *path, const uint16_t *raw, size_t len, uint16_t freq)
{
    if (!g_overwrite && SPIFFS.exists(path))
    {
        Serial.printf("! File exists and overwrite=off: %s\n", path);
        return false;
    }

    JsonDocument doc; // v7
    doc["frequency"] = freq;
    JsonArray arr = doc["raw"].to<JsonArray>(); // v7 way
    for (size_t i = 0; i < len; i++)
        arr.add(raw[i]);

    File f = SPIFFS.open(path, FILE_WRITE); // truncate if exists
    if (!f)
    {
        Serial.printf("! Cannot open for write: %s\n", path);
        return false;
    }
    serializeJson(doc, f);
    f.close();
    Serial.printf("✔ Saved %s (%d items)\n", path, (int)len);
    return true;
}

static inline void dropRepeats(uint32_t ms = 400)
{
    unsigned long t0 = millis();
    while (millis() - t0 < ms)
    {
        if (irrecv.decode(&g_results))
        {
            irrecv.resume(); // discard repeated frames within debounce window
        }
        delay(5);
    }
}

static inline bool learnOnceSavePath(const String &path,
                                     uint16_t defaultFreq = 38000,
                                     uint32_t timeoutMs = 15000)
{
    Serial.printf("[LEARN] Waiting IR, will save to: %s\n", path.c_str());
    unsigned long start = millis();

    while (millis() - start < timeoutMs)
    {
        if (irrecv.decode(&g_results))
        {
            Serial.println("[LEARN] IR received. Details (basic):");
            Serial.println(resultToHumanReadableBasic(&g_results));
            Serial.println("[LEARN] Source code:");
            Serial.println(resultToSourceCode(&g_results));

            if (g_results.rawlen <= 0)
            {
                Serial.println("! Invalid raw length");
                irrecv.resume();
                return false;
            }

            std::vector<uint16_t> raw;
            raw.reserve(g_results.rawlen);
            for (int i = 0; i < g_results.rawlen; i++)
            {
                uint32_t us = g_results.rawbuf[i] * kUsecPerTick; // tick->us
                if (us > 65535)
                    us = 65535;
                raw.push_back((uint16_t)us);
            }

            bool ok = saveJson(path.c_str(), raw.data(), raw.size(), defaultFreq);
            irrecv.resume();

            // Debounce: ignore repeated frames of the same press
            dropRepeats(400);
            return ok;
        }
        delay(10);
    }
    Serial.println("! Timeout: no IR received");
    return false;
}

static inline void listFiles()
{
    File root = SPIFFS.open("/");
    if (!root)
    {
        Serial.println("! SPIFFS root open fail");
        return;
    }
    File file = root.openNextFile();
    Serial.println("Files:");
    while (file)
    {
        Serial.printf("  %s  (%u bytes)\n", file.name(), (unsigned)file.size());
        file = root.openNextFile();
    }
}

// ===================================================
// LED (PWM)
// ===================================================
static inline void pwmInit()
{
    ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
    ledcAttachPin(LED_PIN, PWM_CH);
    ledcWrite(PWM_CH, g_brightness);
    Serial.printf("[LED] PWM initialized (%d Hz, %d-bit, ch=%d) on pin %d\n",
                  PWM_FREQ, PWM_RES, PWM_CH, LED_PIN);
}

static inline void ledSet(int value)
{
    value = constrain(value, 0, 255);
    g_brightness = value;
    ledcWrite(PWM_CH, g_brightness);
    Serial.printf("[LED] Brightness=%d\n", g_brightness);
}

// ===================================================
// Blynk handlers
// ===================================================
BLYNK_WRITE(V5)
{ // brightness 0..255
    int v = param.asInt();
    ledSet(v);
}

BLYNK_WRITE(V6)
{ // absolute target temperature 16..32 step 0.5
    float t = quantizeHalf(param.asFloat());
    Serial.printf("[BLYNK] Target: %.1f°C\n", t);
    g_targetTemp = t;

    // If AC must be ON to accept temp:
    if (!g_isPoweredOn)
    {
        if (g_autoPowerOn)
        {
            if (acPowerOn())
            {
                g_isPoweredOn = true;
                Serial.println("[AC] Sent POWER_ON before applying temperature");
                delay(300); // let AC wake up
            }
            else
            {
                Serial.println("[AC] Missing /ac/POWER_ON.json");
                return;
            }
        }
        else
        {
            Serial.println("[AC] Skipped (AC OFF & autoPowerOn=false)");
            return;
        }
    }

    if (sendAcState(g_mode, t))
    {
        Serial.printf("[AC] Applied: %s %.1f°C\n", g_mode.c_str(), g_targetTemp);
    }
    else
    {
        Serial.printf("[AC] Missing file: /ac/%s_%.1f.json\n", g_mode.c_str(), t);
    }
}

BLYNK_WRITE(V7)
{ // power 0/1
    int on = param.asInt();
    if (on == 1)
    {
        if (acPowerOn())
        {
            g_isPoweredOn = true;
            Serial.println("[BLYNK] AC Power ON");

            float t = quantizeHalf(g_targetTemp);
            if (sendAcState(g_mode, t))
            {
                Serial.printf("[BLYNK] Applied state after ON: %s %.1f°C\n", g_mode.c_str(), t);
            }
            else
            {
                Serial.printf("[BLYNK] Missing: /ac/%s_%.1f.json\n", g_mode.c_str(), t);
            }
        }
        else
        {
            Serial.println("[BLYNK] Missing /ac/POWER_ON.json");
        }
    }
    else
    {
        if (acPowerOff())
        {
            g_isPoweredOn = false;
            Serial.println("[BLYNK] AC Power OFF");
        }
        else
        {
            Serial.println("[BLYNK] Missing /ac/POWER_OFF.json");
        }
    }
}

BLYNK_CONNECTED()
{
    Blynk.syncVirtual(V5, V6, V7);
}

// ===================================================
// Setup / Loop
// ===================================================
void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== controller_node: Blynk + IR send (absolute) + IR recorder ===");

    if (!SPIFFS.begin(true))
    {
        Serial.println("[FS] SPIFFS mount failed");
        while (1)
            delay(1000);
    }

    irsend.begin();
    irrecv.enableIRIn();
    // Optional RX tuning:
    // irrecv.setTolerance(25);         // +/-25%
    // irrecv.setUnknownThreshold(12);  // ignore noise

    pwmInit();

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    Serial.println("[NET] Connecting to WiFi & Blynk...");

    Serial.println("Serial commands:");
    Serial.println("  L ON            -> learn & save /ac/POWER_ON.json");
    Serial.println("  L OFF           -> learn & save /ac/POWER_OFF.json");
    Serial.println("  L COOL <temp>   -> learn & save /ac/COOL_<temp>.json  (temp 16.0..32.0 step 0.5)");
    Serial.println("  ls              -> list files");
    Serial.println("  overwrite on|off");
    Serial.println("  debug on|off    -> toggle IR RX flooding logs");
}

void loop()
{
    Blynk.run();

    // Optional passive debug (flood); off by default
    if (g_debugIr && irrecv.decode(&g_results))
    {
        Serial.println("[Debug] IR frame (not saved):");
        Serial.println(resultToHumanReadableBasic(&g_results));
        Serial.println(resultToSourceCode(&g_results));
        irrecv.resume();
    }

    // Serial command parser
    if (Serial.available())
    {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            return;

        if (line.startsWith("L "))
        {
            String rest = line.substring(2);
            rest.trim();
            if (rest.equalsIgnoreCase("ON"))
            {
                learnOnceSavePath("/ac/POWER_ON.json");
            }
            else if (rest.equalsIgnoreCase("OFF"))
            {
                learnOnceSavePath("/ac/POWER_OFF.json");
            }
            else
            {
                int sp = rest.indexOf(' ');
                if (sp < 0)
                {
                    Serial.println("! Syntax: L ON | L OFF | L COOL <temp>");
                }
                else
                {
                    String mode = rest.substring(0, sp);
                    mode.trim();
                    String sTemp = rest.substring(sp + 1);
                    sTemp.trim();
                    float t = quantizeHalf(sTemp.toFloat());
                    if (!mode.equalsIgnoreCase("COOL"))
                    {
                        Serial.println("! Only COOL mode is supported in this build.");
                    }
                    else if (t < g_minTemp || t > g_maxTemp)
                    {
                        Serial.printf("! Temp out of range (%.1f..%.1f)\n", g_minTemp, g_maxTemp);
                    }
                    else
                    {
                        String path = buildAcStatePath("COOL", t);
                        learnOnceSavePath(path);
                    }
                }
            }
        }
        else if (line == "ls")
        {
            listFiles();
        }
        else if (line.startsWith("overwrite "))
        {
            String arg = line.substring(strlen("overwrite "));
            arg.trim();
            if (arg.equalsIgnoreCase("on"))
            {
                g_overwrite = true;
                Serial.println("✔ overwrite=on");
            }
            else if (arg.equalsIgnoreCase("off"))
            {
                g_overwrite = false;
                Serial.println("✔ overwrite=off");
            }
            else
                Serial.println("! Syntax: overwrite on|off");
        }
        else if (line.startsWith("debug "))
        {
            String arg = line.substring(strlen("debug "));
            arg.trim();
            if (arg.equalsIgnoreCase("on"))
            {
                g_debugIr = true;
                Serial.println("✔ debug=on");
            }
            else if (arg.equalsIgnoreCase("off"))
            {
                g_debugIr = false;
                Serial.println("✔ debug=off");
            }
            else
                Serial.println("! Syntax: debug on|off");
        }
        else
        {
            Serial.println("! Invalid command");
        }
    }
}
