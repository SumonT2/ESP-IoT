#pragma once
// Wi-Fi station with non-blocking reconnect. Never blocks loop(), so the
// button keeps working while the network is down (local-first).

#include <Arduino.h>

void netBegin();
void netLoop();
bool netConnected();
String netIp();
int netRssi();
