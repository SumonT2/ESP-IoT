#pragma once
// Minimal leveled logging. Never log secrets (passwords, keys, tokens).

#include <Arduino.h>

enum LogLevel : uint8_t { LOG_ERROR = 0, LOG_WARN, LOG_INFO, LOG_DEBUG };

extern LogLevel gLogLevel;
const char* logLevelName(LogLevel lvl);

#define LOG_AT(lvl, tag, fmt, ...)                                  \
  do {                                                              \
    if ((lvl) <= gLogLevel) Serial.printf("[" tag "] " fmt "\n", ##__VA_ARGS__); \
  } while (0)

#define LOGE(fmt, ...) LOG_AT(LOG_ERROR, "E", fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG_AT(LOG_WARN,  "W", fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG_AT(LOG_INFO,  "I", fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) LOG_AT(LOG_DEBUG, "D", fmt, ##__VA_ARGS__)
