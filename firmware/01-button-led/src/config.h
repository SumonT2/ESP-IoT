#pragma once
// Per-board values come from build_flags in platformio.ini; defaults here.

#include <Arduino.h>

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
#ifndef BOARD_NAME
#define BOARD_NAME "unknown"
#endif
#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 0
#endif
#ifndef LED_IS_RGB
#define LED_IS_RGB 0
#endif
#ifndef BUTTON_ACTIVE_LOW
#define BUTTON_ACTIVE_LOW 1
#endif

constexpr uint32_t DEBOUNCE_MS   = 30;    // contact bounce filter
constexpr uint32_t LONG_PRESS_MS = 1500;  // hold time for a long press
constexpr uint32_t SAVE_DELAY_MS = 2000;  // coalesce flash writes (wear)
