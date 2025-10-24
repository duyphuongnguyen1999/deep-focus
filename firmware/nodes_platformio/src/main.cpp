#include <Arduino.h>

#ifdef SENSOR_NODE_ESP32
void setupSensorNode();
void loopSensorNode();

void setup()
{
    setupSensorNode();
}
void loop()
{
    loopSensorNode();
}

#elif defined(CONTROLLER_NODE_ESP8266)
void setupControllerNode();
void loopControllerNode();

void setup()
{
    setupControllerNode();
}
void loop()
{
    loopControllerNode();
}

#elif defined(IR_DUMP_ESP8266)
void setupIrDump();
void loopIrDump();

void setup()
{
    setupIrDump();
}
void loop()
{
    loopIrDump();
}

#else
#error "No valid node type defined. Please define SENSOR_NODE_ESP32, CONTROLLER_NODE_ESP8266, or IR_DUMP_ESP8266."
#endif
