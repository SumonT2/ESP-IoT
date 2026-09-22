import fastifyStatic from "@fastify/static";
import websocket from "@fastify/websocket";
import Fastify from "fastify";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

import { bus, type OutMessage } from "./bus.js";
import { config } from "./config.js";
import { listDevices, listEvents } from "./db.js";
import { log } from "./log.js";
import { mqttConnected, mqttStart, mqttStop, sendCommand } from "./mqtt.js";
import { commandSchema } from "./types.js";

const app = Fastify({ logger: false, bodyLimit: 16 * 1024 });
await app.register(websocket);

// Dashboard (Phase 5). Served from the same origin as the API, so no CORS and
// no second port to protect.
const here = dirname(fileURLToPath(import.meta.url));
await app.register(fastifyStatic, { root: join(here, "..", "public") });

app.addHook("onSend", async (_req, reply) => {
  // Defence in depth for the dashboard: no framing, no sniffing, no referrers,
  // and scripts/styles only from this origin.
  reply.header("X-Content-Type-Options", "nosniff");
  reply.header("X-Frame-Options", "DENY");
  reply.header("Referrer-Policy", "no-referrer");
  reply.header(
    "Content-Security-Policy",
    "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self' ws: wss:; img-src 'self' data:; base-uri 'none'; form-action 'none'; frame-ancestors 'none'",
  );
});

const sockets = new Set<import("ws").WebSocket>();

bus.on("out", (msg: OutMessage) => {
  const data = JSON.stringify(msg);
  for (const socket of sockets) {
    if (socket.readyState === socket.OPEN) socket.send(data);
  }
});

app.get("/api/health", async () => ({
  ok: true,
  mqtt: mqttConnected(),
  uptime: Math.round(process.uptime()),
}));

app.get("/api/devices", async () => ({ devices: listDevices() }));

app.get<{ Querystring: { dev?: string; limit?: string } }>("/api/events", async (req) => {
  const limitRaw = Number(req.query.limit ?? 50);
  const limit = Number.isFinite(limitRaw) ? Math.min(Math.max(limitRaw, 1), 500) : 50;
  return { events: listEvents(req.query.dev ?? null, limit) };
});

app.post<{ Params: { id: string } }>("/api/devices/:id/cmd", async (req, reply) => {
  const parsed = commandSchema.safeParse(req.body);
  if (!parsed.success) {
    return reply.code(400).send({ error: "Body must be {\"led\": true|false}" });
  }
  const known = listDevices().some((d) => d.id === req.params.id);
  if (!known) return reply.code(404).send({ error: "Unknown device" });

  // Phase 9 replaces "lan" with the authenticated user from Cloudflare Access.
  if (!sendCommand(req.params.id, parsed.data.led, "lan")) {
    return reply.code(503).send({ error: "MQTT not connected" });
  }
  // 202: the device confirms by publishing its state; the UI waits for that.
  return reply.code(202).send({ accepted: true });
});

app.get("/ws", { websocket: true }, (socket) => {
  sockets.add(socket);
  socket.send(
    JSON.stringify({ type: "snapshot", devices: listDevices(), events: listEvents(null, 50) }),
  );
  socket.on("close", () => sockets.delete(socket));
  socket.on("error", () => sockets.delete(socket));
});

mqttStart();

try {
  await app.listen({ host: config.HTTP_HOST, port: config.HTTP_PORT });
  log.info({ url: `http://${config.HTTP_HOST}:${config.HTTP_PORT}` }, "HTTP listening");
} catch (err) {
  log.error({ err }, "Failed to start HTTP server");
  process.exit(1);
}

async function shutdown(signal: string): Promise<void> {
  log.info({ signal }, "Shutting down");
  await app.close();
  await mqttStop();
  process.exit(0);
}

process.on("SIGINT", () => void shutdown("SIGINT"));
process.on("SIGTERM", () => void shutdown("SIGTERM"));
