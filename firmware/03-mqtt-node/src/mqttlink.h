#pragma once
// MQTT transport. All broker access goes through this module so Phase 8 can
// switch the plain client for a TLS one without touching the rest.

#include <Arduino.h>

void mqttBegin();
void mqttLoop();
bool mqttConnected();

void mqttPublishState();  // retained: last known state for new subscribers
void mqttPublishEvent(const char* type, const char* action, const char* src);
