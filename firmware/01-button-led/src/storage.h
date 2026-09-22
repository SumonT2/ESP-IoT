#pragma once
// Persists device state across power loss.
// ESP32: NVS (Preferences). ESP8266: emulated EEPROM (one flash sector).

#include <Arduino.h>

struct PersistedState {
  uint32_t bootCount = 0;
  bool led = false;
};

void storageBegin();
bool storageLoad(PersistedState& out);  // false if nothing valid stored yet
void storageSave(const PersistedState& in);
