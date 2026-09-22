import Database from "better-sqlite3";
import { mkdirSync } from "node:fs";
import { dirname } from "node:path";

import { config } from "./config.js";
import type { DeviceRow, EventRow } from "./types.js";

mkdirSync(dirname(config.DB_PATH), { recursive: true });

export const db = new Database(config.DB_PATH);
db.pragma("journal_mode = WAL");
db.pragma("foreign_keys = ON");

db.exec(`
CREATE TABLE IF NOT EXISTS devices (
  id         TEXT PRIMARY KEY,
  online     INTEGER NOT NULL DEFAULT 0,
  led        INTEGER,
  src        TEXT,
  boot       INTEGER,
  seq        INTEGER,
  rssi       INTEGER,
  fw         TEXT,
  last_seen  INTEGER,
  updated_at INTEGER
);

CREATE TABLE IF NOT EXISTS events (
  id     INTEGER PRIMARY KEY AUTOINCREMENT,
  dev    TEXT NOT NULL,
  type   TEXT NOT NULL,
  action TEXT NOT NULL,
  src    TEXT NOT NULL,
  boot   INTEGER NOT NULL,
  seq    INTEGER NOT NULL,
  ts     INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_events_dev_ts ON events(dev, ts DESC);

-- Audit trail: every command the server sent, and who asked for it.
CREATE TABLE IF NOT EXISTS commands (
  id      INTEGER PRIMARY KEY AUTOINCREMENT,
  dev     TEXT NOT NULL,
  payload TEXT NOT NULL,
  actor   TEXT NOT NULL,
  ts      INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_commands_dev_ts ON commands(dev, ts DESC);
`);

const upsertDeviceStmt = db.prepare(`
INSERT INTO devices (id, led, src, boot, seq, rssi, fw, last_seen, updated_at, online)
VALUES (@id, @led, @src, @boot, @seq, @rssi, @fw, @ts, @ts, 1)
ON CONFLICT(id) DO UPDATE SET
  led = @led, src = @src, boot = @boot, seq = @seq,
  rssi = COALESCE(@rssi, devices.rssi), fw = COALESCE(@fw, devices.fw),
  last_seen = @ts, updated_at = @ts, online = 1
`);

const setOnlineStmt = db.prepare(`
INSERT INTO devices (id, online, last_seen) VALUES (?, ?, ?)
ON CONFLICT(id) DO UPDATE SET online = excluded.online, last_seen = excluded.last_seen
`);

const getDeviceStmt = db.prepare(`SELECT * FROM devices WHERE id = ?`);
const listDevicesStmt = db.prepare(`SELECT * FROM devices ORDER BY id`);

const insertEventStmt = db.prepare(`
INSERT INTO events (dev, type, action, src, boot, seq, ts)
VALUES (@dev, @type, @action, @src, @boot, @seq, @ts)
`);

const listEventsStmt = db.prepare(`
SELECT * FROM events WHERE (@dev IS NULL OR dev = @dev) ORDER BY id DESC LIMIT @limit
`);

const insertCommandStmt = db.prepare(`
INSERT INTO commands (dev, payload, actor, ts) VALUES (?, ?, ?, ?)
`);

export interface DeviceUpsert {
  id: string;
  led: number;
  src: string;
  boot: number;
  seq: number;
  rssi: number | null;
  fw: string | null;
  ts: number;
}

export function getDevice(id: string): DeviceRow | undefined {
  return getDeviceStmt.get(id) as DeviceRow | undefined;
}

export function listDevices(): DeviceRow[] {
  return listDevicesStmt.all() as DeviceRow[];
}

export function upsertDeviceState(d: DeviceUpsert): void {
  upsertDeviceStmt.run(d);
}

export function setDeviceOnline(id: string, online: boolean, ts: number): void {
  setOnlineStmt.run(id, online ? 1 : 0, ts);
}

export function insertEvent(e: Omit<EventRow, "id">): void {
  insertEventStmt.run(e);
}

export function listEvents(dev: string | null, limit: number): EventRow[] {
  return listEventsStmt.all({ dev, limit }) as EventRow[];
}

export function insertCommand(dev: string, payload: string, actor: string, ts: number): void {
  insertCommandStmt.run(dev, payload, actor, ts);
}
