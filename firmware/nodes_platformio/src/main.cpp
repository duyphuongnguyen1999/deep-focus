#include <Arduino.h>

#ifdef SENSOR_NODE_ESP32
#include "sensor_node_esp32.h"

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
#include "ir_dump_esp8266.h"

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
