#include "mqttlink.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "config.h"
#include "log.h"
#include "net.h"
#include "secrets.h"
#include "state.h"

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

static WiFiClient net;
static PubSubClient client(net);

static char topicState[64];
static char topicEvent[64];
static char topicCmd[64];
static char topicStatus[64];

static uint32_t nextAttempt = 0;
static uint32_t backoffMs = MQTT_RETRY_MIN_MS;

static void onMessage(char* topic, uint8_t* payload, unsigned int len) {
  if (strcmp(topic, topicCmd) != 0) return;  // we only subscribe to our own cmd
  if (len == 0 || len > MQTT_MAX_CMD_BYTES) {
    LOGW("Command ignored: bad length %u", len);
    return;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload, len);
  if (err) {
    LOGW("Command ignored: invalid JSON (%s)", err.c_str());
    return;
  }

  // Allow-list: only {"set":{"led":<bool|0|1>}} is accepted. Anything else
  // is ignored rather than guessed at.
  JsonVariant led = doc["set"]["led"];
  if (led.isNull()) {
    LOGW("Command ignored: no set.led field");
    return;
  }
  if (!led.is<bool>() && !led.is<int>()) {
    LOGW("Command ignored: set.led wrong type");
    return;
  }

  const bool on = led.as<bool>();
  LOGI("Command: led=%d", on);
  if (!stateSetLed(on, Source::Remote)) {
    mqttPublishState();  // already in that state: re-confirm to the server
  }
}

static void publishStatus(const char* status) {
  client.publish(topicStatus, status, true);  // retained
}

static bool connectOnce() {
  LOGI("MQTT connecting to %s:%d as '%s'", MQTT_HOST, MQTT_PORT, MQTT_USER);
  // Last Will: broker publishes "offline" if this device drops without saying
  // goodbye, so the dashboard can show it as unreachable.
  const bool ok = client.connect(DEVICE_ID, MQTT_USER, MQTT_PASS,
                                 topicStatus, 1, true, "offline", true);
  if (!ok) {
    LOGW("MQTT connect failed, rc=%d", client.state());
    return false;
  }
  publishStatus("online");
  client.subscribe(topicCmd, 1);
  mqttPublishState();  // resync: server learns the real state after any outage
  LOGI("MQTT connected, subscribed to %s", topicCmd);
  return true;
}

void mqttBegin() {
  snprintf(topicState, sizeof(topicState), "dev/%s/state", DEVICE_ID);
  snprintf(topicEvent, sizeof(topicEvent), "dev/%s/event", DEVICE_ID);
  snprintf(topicCmd, sizeof(topicCmd), "dev/%s/cmd", DEVICE_ID);
  snprintf(topicStatus, sizeof(topicStatus), "dev/%s/status", DEVICE_ID);

  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(onMessage);
  client.setBufferSize(MQTT_BUFFER_BYTES);
  client.setKeepAlive(MQTT_KEEPALIVE_S);
}

void mqttLoop() {
  if (!netConnected()) return;

  if (client.connected()) {
    client.loop();
    return;
  }
  if (millis() < nextAttempt) return;

  if (connectOnce()) {
    backoffMs = MQTT_RETRY_MIN_MS;
  } else {
    backoffMs = min<uint32_t>(backoffMs * 2, MQTT_RETRY_MAX_MS);
  }
  nextAttempt = millis() + backoffMs;
}

bool mqttConnected() { return client.connected(); }

void mqttPublishState() {
  if (!client.connected()) return;
  const DeviceState& s = stateGet();

  JsonDocument doc;
  doc["dev"] = DEVICE_ID;
  doc["led"] = s.led ? 1 : 0;
  doc["src"] = sourceName(s.src);
  doc["boot"] = s.boot;
  doc["seq"] = s.seq;
  doc["up"] = millis();
  doc["rssi"] = netRssi();
  doc["fw"] = FW_VERSION;

  char buf[MQTT_BUFFER_BYTES];
  const size_t n = serializeJson(doc, buf, sizeof(buf));
  if (!client.publish(topicState, reinterpret_cast<uint8_t*>(buf), n, true)) {
    LOGW("State publish failed");
  }
}

void mqttPublishEvent(const char* type, const char* action, const char* src) {
  if (!client.connected()) return;
  const DeviceState& s = stateGet();

  JsonDocument doc;
  doc["dev"] = DEVICE_ID;
  doc["type"] = type;
  doc["action"] = action;
  doc["src"] = src;
  doc["boot"] = s.boot;
  doc["seq"] = s.seq;
  doc["up"] = millis();

  char buf[MQTT_BUFFER_BYTES];
  const size_t n = serializeJson(doc, buf, sizeof(buf));
  if (!client.publish(topicEvent, reinterpret_cast<uint8_t*>(buf), n, false)) {
    LOGW("Event publish failed");
  }
}
