// Phase 1 — button + LED + local state machine.
//
// Onboard button: click = toggle LED, long press = event only (reserved for
// Wi-Fi setup later). State survives power loss. Serial console simulates
// remote commands until MQTT arrives in Phase 3.

#include <Arduino.h>
#include "button.h"
#include "config.h"
#include "led.h"
#include "log.h"
#include "state.h"

LogLevel gLogLevel = LOG_INFO;

const char* logLevelName(LogLevel lvl) {
  static const char* names[] = {"ERROR", "WARN", "INFO", "DEBUG"};
  return names[lvl];
}

static Button button;

static void printHelp() {
  Serial.println(F("Commands: s=state  t=toggle  1=on  0=off  i=info  v=cycle log level  h=help"));
}

static void printInfo() {
  Serial.printf("INFO fw=%s board=%s led_pin=%d button_pin=%d log=%s free_heap=%lu\n",
                FW_VERSION, BOARD_NAME, LED_PIN, BUTTON_PIN, logLevelName(gLogLevel),
                (unsigned long)ESP.getFreeHeap());
}

static void handleSerial() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    switch (c) {
      case 's': statePrint(); break;
      case 't': stateToggleLed(Source::Serial); break;
      case '1': if (!stateSetLed(true, Source::Serial)) statePrint(); break;
      case '0': if (!stateSetLed(false, Source::Serial)) statePrint(); break;
      case 'i': printInfo(); break;
      case 'v':
        gLogLevel = static_cast<LogLevel>((gLogLevel + 1) % (LOG_DEBUG + 1));
        Serial.printf("Log level: %s\n", logLevelName(gLogLevel));
        break;
      case 'h': case '?': printHelp(); break;
      case '\r': case '\n': case ' ': break;
      default: LOGW("Unknown command '%c' (h for help)", c); break;
    }
  }
}

static void handleButton() {
  switch (button.poll()) {
    case ButtonEvent::Click:
      eventPrint("button", "click", Source::Button);
      stateToggleLed(Source::Button);
      break;
    case ButtonEvent::LongPress:
      eventPrint("button", "long", Source::Button);
      LOGI("Long press (no action yet; reserved for Wi-Fi setup)");
      break;
    case ButtonEvent::None:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // let native-USB serial enumerate
  ledBegin();
  button.begin(BUTTON_PIN, BUTTON_ACTIVE_LOW);
  LOGI("Phase 1 fw %s on %s", FW_VERSION, BOARD_NAME);
  stateBegin();
  statePrint();
  printHelp();
}

void loop() {
  handleButton();
  handleSerial();
  stateLoop();
}
