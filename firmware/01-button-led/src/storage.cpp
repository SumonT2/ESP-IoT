#include "storage.h"
#include "log.h"

#if defined(ESP32)
#include <Preferences.h>

static Preferences prefs;

void storageBegin() {
  if (!prefs.begin("devstate", false)) LOGE("NVS open failed");
}

bool storageLoad(PersistedState& out) {
  if (!prefs.isKey("boot")) return false;
  out.bootCount = prefs.getUInt("boot", 0);
  out.led = prefs.getUChar("led", 0) != 0;
  return true;
}

void storageSave(const PersistedState& in) {
  prefs.putUInt("boot", in.bootCount);
  prefs.putUChar("led", in.led ? 1 : 0);
}

#elif defined(ESP8266)
#include <EEPROM.h>

// Magic value guards against reading garbage from a fresh or foreign layout.
// Bump the low byte whenever the Record layout changes.
static constexpr uint32_t MAGIC = 0x53544101;  // "STA" + v1

struct Record {
  uint32_t magic;
  uint32_t bootCount;
  uint8_t led;
};

void storageBegin() {
  EEPROM.begin(sizeof(Record));
}

bool storageLoad(PersistedState& out) {
  Record r;
  EEPROM.get(0, r);
  if (r.magic != MAGIC) return false;
  out.bootCount = r.bootCount;
  out.led = r.led != 0;
  return true;
}

void storageSave(const PersistedState& in) {
  Record r{MAGIC, in.bootCount, static_cast<uint8_t>(in.led ? 1 : 0)};
  EEPROM.put(0, r);
  if (!EEPROM.commit()) LOGE("EEPROM commit failed");
}

#else
#error "Unsupported platform"
#endif
