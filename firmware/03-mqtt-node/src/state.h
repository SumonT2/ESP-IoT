#pragma once
// Single source of truth for device state. Every change goes through here,
// gets a sequence number and a source, and is emitted as JSON. In Phase 3
// the same JSON is published to MQTT (dev/<id>/state and dev/<id>/event).

#include <Arduino.h>

enum class Source : uint8_t { Boot, Button, Serial, Remote };

struct DeviceState {
  bool led = false;
  Source src = Source::Boot;  // who caused the last change
  uint32_t boot = 0;          // persisted boot counter
  uint32_t seq = 0;           // change counter, resets each boot
};                            // ordering key for the server: (boot, seq)

const char* sourceName(Source s);

void stateBegin();  // load persisted state, bump boot counter, apply outputs
void stateLoop();   // deferred flash save
const DeviceState& stateGet();

bool stateSetLed(bool on, Source src);  // false if already in that state
void stateToggleLed(Source src);

void statePrint();  // STATE {...}
void eventPrint(const char* type, const char* action, Source src);  // EVENT {...}

// Called on every accepted change; used to publish to MQTT. Set in setup().
using StateChangeHandler = void (*)();
void stateOnChange(StateChangeHandler handler);
