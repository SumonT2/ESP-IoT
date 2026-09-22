import "dotenv/config";
import { z } from "zod";

const schema = z.object({
  MQTT_HOST: z.string().min(1),
  MQTT_PORT: z.coerce.number().int().positive().default(1883),
  MQTT_USER: z.string().min(1),
  MQTT_PASS: z.string().min(1),
  HTTP_HOST: z.string().min(1).default("127.0.0.1"),
  HTTP_PORT: z.coerce.number().int().positive().default(8080),
  DB_PATH: z.string().min(1).default("./data/iot.db"),
  LOG_LEVEL: z.enum(["fatal", "error", "warn", "info", "debug", "trace"]).default("info"),
});

const parsed = schema.safeParse(process.env);
if (!parsed.success) {
  // Fail fast and loudly: a half-configured server is worse than none.
  console.error("Invalid configuration:", parsed.error.flatten().fieldErrors);
  process.exit(1);
}

export const config = parsed.data;
