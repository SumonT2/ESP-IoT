import mqtt, { type MqttClient } from "mqtt";

import { publish } from "./bus.js";
import { config } from "./config.js";
import {
  getDevice,
  insertCommand,
  insertEvent,
  setDeviceOnline,
  upsertDeviceState,
} from "./db.js";
import { log } from "./log.js";
import { deviceEventSchema, deviceStateSchema } from "./types.js";

let client: MqttClient | null = null;

const TOPIC_STATE = "dev/+/state";
const TOPIC_EVENT = "dev/+/event";
const TOPIC_STATUS = "dev/+/status";

/** dev/<id>/<leaf> -> id, or null if the topic is not ours. */
function deviceIdFromTopic(topic: string, leaf: string): string | null {
  const parts = topic.split("/");
  if (parts.length !== 3 || parts[0] !== "dev" || parts[2] !== leaf) return null;
  return parts[1] ?? null;
}

/**
 * Messages can arrive out of order or be replayed (retained). (boot, seq) is
 * the device's own ordering key, so anything older than what we have is stale.
 */
function isStale(id: string, boot: number, seq: number): boolean {
  const current = getDevice(id);
  if (!current || current.boot === null || current.seq === null) return false;
  if (boot > current.boot) return false;
  if (boot < current.boot) return true;
  return seq < current.seq;
}

function handleState(topic: string, raw: Buffer): void {
  const id = deviceIdFromTopic(topic, "state");
  if (!id) return;

  const parsed = deviceStateSchema.safeParse(safeJson(raw));
  if (!parsed.success) {
    log.warn({ topic }, "Ignored malformed state message");
    return;
  }
  const msg = parsed.data;
  if (msg.dev !== id) {
    // Topic and payload disagree: a device may only speak for itself.
    log.warn({ topic, claimed: msg.dev }, "Ignored state with mismatched device id");
    return;
  }
  if (isStale(id, msg.boot, msg.seq)) {
    log.debug({ id, boot: msg.boot, seq: msg.seq }, "Ignored stale state");
    return;
  }

  const ts = Date.now();
  upsertDeviceState({
    id,
    led: msg.led,
    src: msg.src,
    boot: msg.boot,
    seq: msg.seq,
    rssi: msg.rssi ?? null,
    fw: msg.fw ?? null,
    ts,
  });
  const device = getDevice(id);
  if (device) publish({ type: "device", device });
}

function handleEvent(topic: string, raw: Buffer): void {
  const id = deviceIdFromTopic(topic, "event");
  if (!id) return;

  const parsed = deviceEventSchema.safeParse(safeJson(raw));
  if (!parsed.success) {
    log.warn({ topic }, "Ignored malformed event message");
    return;
  }
  const msg = parsed.data;
  if (msg.dev !== id) {
    log.warn({ topic, claimed: msg.dev }, "Ignored event with mismatched device id");
    return;
  }

  const row = {
    dev: id,
    type: msg.type,
    action: msg.action,
    src: msg.src,
    boot: msg.boot,
    seq: msg.seq,
    ts: Date.now(),
  };
  insertEvent(row);
  publish({ type: "event", event: { id: 0, ...row } });
}

function handleStatus(topic: string, raw: Buffer): void {
  const id = deviceIdFromTopic(topic, "status");
  if (!id) return;

  const value = raw.toString("utf8").trim();
  if (value !== "online" && value !== "offline") {
    log.warn({ topic, value }, "Ignored unknown status value");
    return;
  }
  setDeviceOnline(id, value === "online", Date.now());
  const device = getDevice(id);
  if (device) publish({ type: "device", device });
  log.info({ id, status: value }, "Device status");
}

function safeJson(raw: Buffer): unknown {
  try {
    return JSON.parse(raw.toString("utf8"));
  } catch {
    return null;
  }
}

export function mqttStart(): void {
  client = mqtt.connect({
    host: config.MQTT_HOST,
    port: config.MQTT_PORT,
    username: config.MQTT_USER,
    password: config.MQTT_PASS,
    clientId: `backend-${process.pid}`,
    clean: true,
    reconnectPeriod: 2000,
    // Phase 8 adds: protocol "mqtts", CA cert, client cert (mTLS).
  });

  client.on("connect", () => {
    log.info({ host: config.MQTT_HOST, port: config.MQTT_PORT }, "MQTT connected");
    client?.subscribe([TOPIC_STATE, TOPIC_EVENT, TOPIC_STATUS], { qos: 1 }, (err) => {
      if (err) log.error({ err }, "MQTT subscribe failed");
    });
  });

  client.on("message", (topic, payload) => {
    if (topic.endsWith("/state")) handleState(topic, payload);
    else if (topic.endsWith("/event")) handleEvent(topic, payload);
    else if (topic.endsWith("/status")) handleStatus(topic, payload);
  });

  client.on("error", (err) => log.error({ err }, "MQTT error"));
  client.on("reconnect", () => log.warn("MQTT reconnecting"));
  client.on("close", () => log.warn("MQTT connection closed"));
}

/** Sends a command to one device and records it in the audit trail. */
export function sendCommand(dev: string, led: boolean, actor: string): boolean {
  if (!client?.connected) return false;
  const payload = JSON.stringify({ set: { led } });
  client.publish(`dev/${dev}/cmd`, payload, { qos: 1, retain: false });
  insertCommand(dev, payload, actor, Date.now());
  log.info({ dev, led, actor }, "Command sent");
  return true;
}

export function mqttConnected(): boolean {
  return client?.connected === true;
}

export async function mqttStop(): Promise<void> {
  await client?.endAsync();
  client = null;
}
