#pragma once
#include <Arduino.h>

enum class WaitMode
{
    Idle,
    WaitingForName,
    WaitingForIr
};

void setupIrDump();
void loopIrDump();
void setWaitMode(WaitMode m);
void setPendingButton(String &s);
