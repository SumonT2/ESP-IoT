#pragma once
// Debounced push button with click / long-press detection. Non-blocking.

#include <Arduino.h>

enum class ButtonEvent : uint8_t { None, Click, LongPress };

class Button {
 public:
  void begin(uint8_t pin, bool activeLow);
  ButtonEvent poll();  // call every loop()
  bool isPressed() const { return stable_; }

 private:
  bool readRaw() const;

  uint8_t pin_ = 0;
  bool activeLow_ = true;
  bool lastRaw_ = false;
  bool stable_ = false;
  bool longFired_ = false;
  uint32_t lastChange_ = 0;
  uint32_t pressStart_ = 0;
};
