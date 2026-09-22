import { EventEmitter } from "node:events";

import type { DeviceRow, EventRow } from "./types.js";

/** Messages pushed to browsers over the WebSocket. */
export type OutMessage =
  | { type: "snapshot"; devices: DeviceRow[]; events: EventRow[] }
  | { type: "device"; device: DeviceRow }
  | { type: "event"; event: EventRow };

/** Decouples MQTT ingest from WebSocket fan-out. */
export const bus = new EventEmitter();

export function publish(msg: OutMessage): void {
  bus.emit("out", msg);
}
