// Phase 3 — Wi-Fi + MQTT node.
//
// Button press -> LED toggles instantly (local-first) -> state published to
// the broker. Server command on dev/<id>/cmd -> LED changes -> state published
// back. The server only ever learns state the device has confirmed.

#include <Arduino.h>

#include "button.h"
#include "config.h"
#include "led.h"
#include "log.h"
#include "mqttlink.h"
#include "net.h"
#include "secrets.h"
#include "state.h"

LogLevel gLogLevel = LOG_INFO;

const char* logLevelName(LogLevel lvl) {
  static const char* names[] = {"ERROR", "WARN", "INFO", "DEBUG"};
  return names[lvl];
}

static Button button;

static void onStateChanged() {
  const DeviceState& s = stateGet();
  // History entry for the change itself, so the dashboard shows who did what
  // (manual vs remote), not just button presses.
  mqttPublishEvent("led", s.led ? "on" : "off", sourceName(s.src));
  mqttPublishState();
}

static void printHelp() {
  Serial.println(F("Commands: s=state  t=toggle  1=on  0=off  n=network  i=info  v=log level  h=help"));
}

static void printNetwork() {
  Serial.printf("NET wifi=%s ip=%s rssi=%d mqtt=%s host=%s:%d dev=%s\n",
                netConnected() ? "up" : "down", netIp().c_str(), netRssi(),
                mqttConnected() ? "up" : "down", MQTT_HOST, MQTT_PORT, DEVICE_ID);
}

static void printInfo() {
  Serial.printf("INFO fw=%s board=%s dev=%s led_pin=%d button_pin=%d log=%s free_heap=%lu\n",
                FW_VERSION, BOARD_NAME, DEVICE_ID, LED_PIN, BUTTON_PIN,
                logLevelName(gLogLevel), (unsigned long)ESP.getFreeHeap());
}

static void handleSerial() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    switch (c) {
      case 's': statePrint(); break;
      case 't': stateToggleLed(Source::Serial); break;
      case '1': if (!stateSetLed(true, Source::Serial)) statePrint(); break;
      case '0': if (!stateSetLed(false, Source::Serial)) statePrint(); break;
      case 'n': printNetwork(); break;
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
      mqttPublishEvent("button", "click", "button");
      stateToggleLed(Source::Button);
      break;
    case ButtonEvent::LongPress:
      eventPrint("button", "long", Source::Button);
      mqttPublishEvent("button", "long", "button");
      break;
    case ButtonEvent::None:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  ledBegin();
  button.begin(BUTTON_PIN, BUTTON_ACTIVE_LOW);
  LOGI("Phase 3 fw %s on %s (device '%s')", FW_VERSION, BOARD_NAME, DEVICE_ID);
  stateBegin();
  stateOnChange(onStateChanged);
  statePrint();
  netBegin();
  mqttBegin();
  printHelp();
}

void loop() {
  handleButton();   // always first: works with no network
  handleSerial();
  stateLoop();
  netLoop();
  mqttLoop();
}
