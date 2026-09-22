import { z } from "zod";

/** Device -> server state message (retained on dev/<id>/state). */
export const deviceStateSchema = z.object({
  dev: z.string().min(1).max(64),
  led: z.union([z.literal(0), z.literal(1), z.boolean()]).transform((v) => (v === true || v === 1 ? 1 : 0)),
  src: z.enum(["boot", "button", "serial", "remote"]).catch("boot"),
  boot: z.number().int().nonnegative(),
  seq: z.number().int().nonnegative(),
  up: z.number().int().nonnegative().optional(),
  rssi: z.number().int().optional(),
  fw: z.string().max(32).optional(),
});
export type DeviceStateMsg = z.infer<typeof deviceStateSchema>;

/** Device -> server event message (dev/<id>/event). */
export const deviceEventSchema = z.object({
  dev: z.string().min(1).max(64),
  type: z.string().min(1).max(32),
  action: z.string().min(1).max(32),
  src: z.enum(["boot", "button", "serial", "remote"]).catch("boot"),
  boot: z.number().int().nonnegative(),
  seq: z.number().int().nonnegative(),
  up: z.number().int().nonnegative().optional(),
});
export type DeviceEventMsg = z.infer<typeof deviceEventSchema>;

/** Browser -> server command body. */
export const commandSchema = z.object({
  led: z.boolean(),
});

export interface DeviceRow {
  id: string;
  online: number;
  led: number | null;
  src: string | null;
  boot: number | null;
  seq: number | null;
  rssi: number | null;
  fw: string | null;
  last_seen: number | null;
  updated_at: number | null;
}

export interface EventRow {
  id: number;
  dev: string;
  type: string;
  action: string;
  src: string;
  boot: number;
  seq: number;
  ts: number;
}
