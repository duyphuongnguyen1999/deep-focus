#include <Arduino.h>
#include <ArduinoJson.h>

#include "ir_dump_esp8266.h"

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>

const uint16_t kRecvPin = 14; // D5 (GPIO14)
const uint32_t kBaudRate = 115200;
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 50; // in milliseconds
const uint8_t kMinUnknownSize = 12;
const uint8_t kTolerancePercentage = kTolerance;

// Use turn on the save buffer feature for more complete capture coverage.
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
decode_results results; // Somewhere to store the results

enum class WaitMode
{
    Idle,
    WaitingForName,
    WaitingForIr
};
static WaitMode waitMode = WaitMode::WaitingForName;
static String pendingButton;

void setWaitMode(WaitMode m)
{
    waitMode = m;
}

void setPendingButton(const String &s)
{
    pendingButton = s;
}

void setupIrDump()
{
    Serial.begin(kBaudRate);
    while (!Serial)
        delay(1000);

    Serial.printf("\n[BOOT] IR recv on pin %u, timeout=%ums, buf=%u\n",
                  kRecvPin, (unsigned)kTimeoutMs, (unsigned)kCaptureBufferSize);

#if DECODE_HASH
    // Ignore messages with less than minimum on or off pulses.
    irrecv.setUnknownThreshold(kMinUnknownSize);
#endif                                         // DECODE_HASH
    irrecv.setTolerance(kTolerancePercentage); // Override the default tolerance.
    irrecv.enableIRIn();                       // Start the receiver
}

void loopIrDump()
{
    if (waitMode == WaitMode::WaitingForName)
    {
        if (Serial.available())
        {
            String s = Serial.readStringUntil('\n');
            s.trim();
            if (s.length() > 0)
            {
                pendingButton = s;
                Serial.printf("[INFO] Ready to receive IR code for button name: '%s'\n", pendingButton.c_str());
                waitMode = WaitMode::WaitingForIr;
            }
            else
            {
                Serial.printf("[INFO] Please enter a button name.\n");
            }
        }
        return;
    }
    if (waitMode == WaitMode::WaitingForIr)
    {
        // Check if the IR code has been received.
        if (irrecv.decode(&results))
        {
            String valueHex;
            if (results.decode_type != decode_type_t::UNKNOWN)
            {
                char buf[22];
                snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)results.value);
                valueHex = String(buf);
            }

            JsonDocument doc;
            doc["btn"] = pendingButton;
            doc["ts_ms"] = millis();
            doc["protocol"] = typeToString(results.decode_type, false);
            doc["bits"] = results.bits;

            if (results.decode_type != decode_type_t::UNKNOWN)
            {
                char buf[22];
                snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)results.value);
                doc["value"] = buf;
            }

            JsonArray raw = doc["raw_us"].to<JsonArray>();

            for (uint16_t i = 0; i < results.rawlen; i++)
            {
                raw.add((uint32_t)results.rawbuf[i] * kRawTick);
            }

            // In JSON
            serializeJson(doc, Serial);
            Serial.println();

            // // Display the basic output of what we found.
            // Serial.print(resultToHumanReadableBasic(&results));
            // // Display any extra A/C info if we have it.
            // String description = IRAcUtils::resultAcToString(&results);
            // if (description.length())
            //     Serial.println(D_STR_MESGDESC ": " + description);
            // yield(); // Feed the WDT as the text output can take a while to print.
            // // Output the results as source code
            // Serial.println(resultToSourceCode(&results));
            // Serial.println(); // Blank line between entries
            // yield();          // Feed the WDT (again)

            irrecv.resume();
            waitMode = WaitMode::WaitingForName;
            pendingButton = "";
            Serial.printf("[INFO] Please enter a button name.\n");
        }
    }
}
