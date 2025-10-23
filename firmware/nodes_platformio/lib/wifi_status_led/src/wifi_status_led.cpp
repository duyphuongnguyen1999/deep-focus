#include "wifi_status_led.h"
#include <Arduino.h>

void setupWifiStatusLed()
{
    pinMode(INBOARD_LED_PIN, OUTPUT);
    digitalWrite(INBOARD_LED_PIN, LED_OFF);
}

void updateWifiStatusLed(WifiStatus status)
{
    switch (status)
    {
    case WifiStatus::WAITING_FOR_PROVISIONING:
        digitalWrite(INBOARD_LED_PIN, LED_ON);
        delay(2000);
        digitalWrite(INBOARD_LED_PIN, LED_OFF);
        break;
    case WifiStatus::CONNECTED_TO_ROUTER:
        digitalWrite(INBOARD_LED_PIN, LED_ON);
        break;
    case WifiStatus::RECEIVING_CONFIG:
        // Blink the LED to indicate receiving configuration
        for (int i = 0; i < 3; ++i)
        {
            digitalWrite(INBOARD_LED_PIN, LED_ON);
            delay(200);
            digitalWrite(INBOARD_LED_PIN, LED_OFF);
            delay(200);
        }
        break;
    }
}