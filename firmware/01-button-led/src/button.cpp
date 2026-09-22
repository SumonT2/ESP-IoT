#include "button.h"
#include "config.h"

void Button::begin(uint8_t pin, bool activeLow) {
  pin_ = pin;
  activeLow_ = activeLow;
  pinMode(pin_, activeLow_ ? INPUT_PULLUP : INPUT);
  lastRaw_ = stable_ = readRaw();
  lastChange_ = millis();
}

bool Button::readRaw() const {
  return (digitalRead(pin_) == LOW) == activeLow_;
}

ButtonEvent Button::poll() {
  const uint32_t now = millis();
  const bool raw = readRaw();

  if (raw != lastRaw_) {  // contact moved: restart debounce window
    lastRaw_ = raw;
    lastChange_ = now;
  }

  if (now - lastChange_ >= DEBOUNCE_MS && raw != stable_) {
    stable_ = raw;
    if (stable_) {  // pressed
      pressStart_ = now;
      longFired_ = false;
    } else if (!longFired_) {  // released before long-press threshold
      return ButtonEvent::Click;
    }
  }

  if (stable_ && !longFired_ && now - pressStart_ >= LONG_PRESS_MS) {
    longFired_ = true;
    return ButtonEvent::LongPress;
  }
  return ButtonEvent::None;
}
