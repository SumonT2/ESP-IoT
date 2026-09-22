#include "state.h"
#include "config.h"
#include "led.h"
#include "log.h"
#include "storage.h"

static DeviceState state;
static bool dirty = false;
static uint32_t dirtySince = 0;
static StateChangeHandler onChange = nullptr;

void stateOnChange(StateChangeHandler handler) { onChange = handler; }

const char* sourceName(Source s) {
  switch (s) {
    case Source::Boot:   return "boot";
    case Source::Button: return "button";
    case Source::Serial: return "serial";
    case Source::Remote: return "remote";
  }
  return "unknown";
}

static PersistedState toPersisted() {
  PersistedState p;
  p.bootCount = state.boot;
  p.led = state.led;
  return p;
}

void stateBegin() {
  storageBegin();
  PersistedState p;
  if (storageLoad(p)) {
    state.boot = p.bootCount + 1;
    state.led = p.led;
    LOGI("Restored state: led=%d (previous boot #%lu)", state.led, (unsigned long)p.bootCount);
  } else {
    state.boot = 1;
    LOGI("No saved state, using defaults");
  }
  state.src = Source::Boot;
  state.seq = 0;
  ledSet(state.led);
  storageSave(toPersisted());  // record the new boot counter immediately
}

void stateLoop() {
  if (dirty && millis() - dirtySince >= SAVE_DELAY_MS) {
    storageSave(toPersisted());
    dirty = false;
    LOGD("State saved to flash");
  }
}

const DeviceState& stateGet() { return state; }

bool stateSetLed(bool on, Source src) {
  if (state.led == on) {
    LOGD("LED already %s, no change", on ? "on" : "off");
    return false;
  }
  state.led = on;
  state.src = src;
  state.seq++;
  ledSet(on);

  // Coalesce writes: rapid toggles cause one flash write, not many.
  dirty = true;
  dirtySince = millis();

  eventPrint("led", on ? "on" : "off", src);
  statePrint();
  if (onChange) onChange();  // publish to MQTT if connected
  return true;
}

void stateToggleLed(Source src) { stateSetLed(!state.led, src); }

void statePrint() {
  Serial.printf("STATE {\"dev\":\"%s\",\"led\":%d,\"src\":\"%s\",\"boot\":%lu,\"seq\":%lu,\"up\":%lu}\n",
                BOARD_NAME, state.led ? 1 : 0, sourceName(state.src),
                (unsigned long)state.boot, (unsigned long)state.seq, (unsigned long)millis());
}

void eventPrint(const char* type, const char* action, Source src) {
  Serial.printf("EVENT {\"dev\":\"%s\",\"type\":\"%s\",\"action\":\"%s\",\"src\":\"%s\",\"boot\":%lu,\"seq\":%lu,\"up\":%lu}\n",
                BOARD_NAME, type, action, sourceName(src),
                (unsigned long)state.boot, (unsigned long)state.seq, (unsigned long)millis());
}
