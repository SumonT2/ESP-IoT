// Phase 0 — board check.
// Prints chip identity (confirms which board you actually have) and blinks the LED.

#include <Arduino.h>
#if defined(ESP32)
#include <esp_mac.h>
#endif

#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 0
#endif
#ifndef LED_IS_RGB
#define LED_IS_RGB 0
#endif

static void setLed(bool on) {
#if LED_IS_RGB
  // Addressable RGB LED (WS2812) — Arduino-ESP32 core 3.x helper.
  rgbLedWrite(LED_PIN, 0, on ? 20 : 0, 0);  // dim green
#else
  digitalWrite(LED_PIN, (on ^ LED_ACTIVE_LOW) ? HIGH : LOW);
#endif
}

static void printChipInfo() {
  Serial.println();
  Serial.println(F("===== Board check ====="));
#if defined(ESP32)
  Serial.printf("Chip model   : %s rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("CPU cores    : %d @ %lu MHz\n", ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("Flash size   : %lu KB\n", (unsigned long)(ESP.getFlashChipSize() / 1024));
  Serial.printf("Free heap    : %lu B\n", (unsigned long)ESP.getFreeHeap());
  Serial.printf("PSRAM        : %lu B\n", (unsigned long)ESP.getPsramSize());
  Serial.printf("SDK          : %s\n", ESP.getSdkVersion());
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("Wi-Fi MAC    : %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#elif defined(ESP8266)
  Serial.println(F("Chip model   : ESP8266"));
  Serial.printf("Chip ID      : %06X\n", ESP.getChipId());
  Serial.printf("CPU          : %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash (real) : %u KB\n", ESP.getFlashChipRealSize() / 1024);
  Serial.printf("Free heap    : %u B\n", ESP.getFreeHeap());
  Serial.printf("Core         : %s\n", ESP.getCoreVersion().c_str());
  Serial.printf("SDK          : %s\n", ESP.getSdkVersion());
#endif
  Serial.printf("LED pin      : GPIO%d\n", LED_PIN);
  Serial.println(F("======================="));
}

void setup() {
  Serial.begin(115200);
#if !LED_IS_RGB
  pinMode(LED_PIN, OUTPUT);
#endif
  setLed(false);
  delay(1500);  // give native-USB serial time to enumerate
  printChipInfo();
}

void loop() {
  static uint32_t lastBlink = 0;
  static uint32_t lastInfo = 0;
  static bool on = false;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    on = !on;
    setLed(on);
  }
  // Re-print periodically: native-USB boards (C3 Super Mini) drop the USB
  // connection on reset, so the monitor misses the one-time boot print.
  if (millis() - lastInfo >= 5000) {
    lastInfo = millis();
    printChipInfo();
  }
}
