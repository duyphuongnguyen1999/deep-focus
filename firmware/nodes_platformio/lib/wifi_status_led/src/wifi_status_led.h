#pragma once

#define INBOARD_LED_PIN 2
#define LED_ON LOW
#define LED_OFF HIGH

enum class WifiStatus
{
    WAITING_FOR_PROVISIONING,
    CONNECTED_TO_ROUTER,
    RECEIVING_CONFIG,
};

void setupWifiStatusLed();
void updateWifiStatusLed(WifiStatus status);