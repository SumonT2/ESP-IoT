#include "net.h"
#include "config.h"
#include "log.h"
#include "secrets.h"

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

static uint32_t nextAttempt = 0;
static uint32_t lastBegin = 0;
static uint32_t backoffMs = WIFI_RETRY_MIN_MS;
static bool wasConnected = false;

void netBegin() {
  WiFi.persistent(false);  // keep credentials out of flash (wear + exposure)
  WiFi.mode(WIFI_STA);
#if defined(ESP32)
  WiFi.setHostname(DEVICE_ID);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // lower latency for commands
#elif defined(ESP8266)
  WiFi.hostname(DEVICE_ID);
  WiFi.setAutoReconnect(true);
#endif
  LOGI("Wi-Fi connecting to '%s'", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastBegin = millis();
  nextAttempt = lastBegin + backoffMs;
}

void netLoop() {
  const bool up = WiFi.status() == WL_CONNECTED;

  if (up != wasConnected) {
    wasConnected = up;
    if (up) {
      backoffMs = WIFI_RETRY_MIN_MS;
      LOGI("Wi-Fi up: ip=%s rssi=%d dBm", netIp().c_str(), netRssi());
    } else {
      LOGW("Wi-Fi lost");
    }
  }
  if (up) return;

  // Never interrupt an attempt that is still running: the driver rejects a
  // reconfigure while connecting (ESP_ERR_WIFI_STATE).
  if (millis() - lastBegin < WIFI_CONNECT_TIMEOUT_MS) return;

  if (millis() >= nextAttempt) {  // retry with backoff, never blocking
    LOGD("Wi-Fi retry (backoff %lu ms)", (unsigned long)backoffMs);
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    lastBegin = millis();
    backoffMs = min<uint32_t>(backoffMs * 2, WIFI_RETRY_MAX_MS);
    nextAttempt = lastBegin + backoffMs;
  }
}

bool netConnected() { return WiFi.status() == WL_CONNECTED; }
String netIp() { return WiFi.localIP().toString(); }
int netRssi() { return WiFi.RSSI(); }
