#include "led.h"
#include "config.h"

void ledBegin() {
#if !LED_IS_RGB
  pinMode(LED_PIN, OUTPUT);
#endif
  ledSet(false);
}

void ledSet(bool on) {
#if LED_IS_RGB
  rgbLedWrite(LED_PIN, 0, on ? 20 : 0, 0);  // dim green
#else
  digitalWrite(LED_PIN, (on ^ LED_ACTIVE_LOW) ? HIGH : LOW);
#endif
}
