#include <Arduino.h>
#include <assert.h>
#include <IRremoteESP8266.h>

#include <IRrecv.h>
#include <IRutils.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>

const uint16_t kRecvPin = 14; // D5 (GPIO14)
const uint32_t kBaudRate = 9600;
const uint16_t kCaptureBufferSize = 1024;
const uint8_t kTimeout = 50; // in milliseconds
const uint8_t kMinUnknownSize = 12;
const uint8_t kTolerancePercentage = kTolerance;

// Use turn on the save buffer feature for more complete capture coverage.
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
decode_results results; // Somewhere to store the results

void setupIrDump()
{
    Serial.begin(115200);
    while (!Serial)
        delay(1000);
    // Perform a low level sanity checks that the compiler performs bit field
    // packing as we expect and Endianness is as we expect.
    assert(irutils::lowLevelSanityCheck() == 0);

    Serial.printf("\n" D_STR_IRRECVDUMP_STARTUP "\n", kRecvPin);

#if DECODE_HASH
    // Ignore messages with less than minimum on or off pulses.
    irrecv.setUnknownThreshold(kMinUnknownSize);
#endif                                         // DECODE_HASH
    irrecv.setTolerance(kTolerancePercentage); // Override the default tolerance.
    irrecv.enableIRIn();                       // Start the receiver
}

void loopIrDump()
{
    // Check if the IR code has been received.
    if (irrecv.decode(&results))
    {
        // Display a crude timestamp.
        uint32_t now = millis();
        Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);
        // Check if we got an IR message that was to big for our capture buffer.
        if (results.overflow)
            Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);
        // Display the library version the message was captured with.
        Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_STR "\n");
        // Display the tolerance percentage if it has been change from the default.
        if (kTolerancePercentage != kTolerance)
            Serial.printf(D_STR_TOLERANCE " : %d%%\n", kTolerancePercentage);
        // Display the basic output of what we found.
        Serial.print(resultToHumanReadableBasic(&results));
        // Display any extra A/C info if we have it.
        String description = IRAcUtils::resultAcToString(&results);
        if (description.length())
            Serial.println(D_STR_MESGDESC ": " + description);
        yield(); // Feed the WDT as the text output can take a while to print.
        // Output the results as source code
        Serial.println(resultToSourceCode(&results));
        Serial.println(); // Blank line between entries
        yield();          // Feed the WDT (again)
    }
}
