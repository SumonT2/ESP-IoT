# Phase 4 — Backend (Node.js + TypeScript)

Bridges MQTT to a database and a live WebSocket feed. It is the only component
that talks to the broker on the server side, and the only source the dashboard
(Phase 5) reads from.

```
boards ──MQTT──> Mosquitto ──subscribe──> backend ──WebSocket──> browser
                                   ▲            │
                                   └──publish───┘  commands + audit log
```

## Where to run it

Clone the repo **inside WSL** (`~/ESP-IoT`), not under `/mnt/c`. Files on the
Windows drive are much slower from Linux, and your Windows copy sits in
OneDrive, which should not sync `node_modules`.

```bash
cd ~
git clone https://github.com/SumonT2/ESP-IoT.git
cd ESP-IoT/server/backend
```

## 1. Install Node.js (20 or newer)

```bash
sudo apt install -y nodejs npm build-essential python3
node -v
```

`build-essential` and `python3` are needed if `better-sqlite3` has no prebuilt
binary for your Node version. If `node -v` is older than v20, install a current
version with [nvm](https://github.com/nvm-sh/nvm) instead.

## 2. Configure

```bash
cp .env.example .env
nano .env
```

Set `MQTT_PASS` to the **`backend`** password from Phase 2 step B3. `.env` is
git-ignored — never commit it.

Defaults worth knowing:
- `MQTT_HOST=127.0.0.1` — broker is on the same machine.
- `HTTP_HOST=127.0.0.1` — **loopback only**. Nothing on the LAN can reach the
  API yet, because it has no login. Windows can still open
  `http://localhost:8080` thanks to WSL mirrored networking. The gate comes in
  Phase 6 (Cloudflare Access) and Phase 9 (app login).

## 3. Install and run

```bash
npm install
npm run dev      # watch mode, for development
```

Expected:

```
MQTT connected {"host":"127.0.0.1","port":1883}
HTTP listening {"url":"http://127.0.0.1:8080"}
Device status {"id":"esp32c6","status":"online"}
```

## 4. Test

```bash
curl -s localhost:8080/api/health              # {"ok":true,"mqtt":true,...}
curl -s localhost:8080/api/devices | jq        # last known state per board
curl -s 'localhost:8080/api/events?limit=5' | jq
```

Turn an LED on through the API (this is what the dashboard button will do):

```bash
curl -s -X POST localhost:8080/api/devices/esp32c6/cmd \
     -H 'content-type: application/json' -d '{"led":true}'
```

Returns `{"accepted":true}` with status **202**, meaning "sent, not yet
confirmed". Press the board's button and re-run `/api/devices`: `src` flips to
`button`. That is manual and remote control sharing one source of truth.

## 5. Run it as a service (after testing)

```bash
npm run build
nano iot-backend.service          # check User= and paths
sudo cp iot-backend.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now iot-backend
systemctl status iot-backend --no-pager
journalctl -u iot-backend -f
```

## API

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/health` | Liveness + MQTT connection state |
| GET | `/api/devices` | Last confirmed state of every device |
| GET | `/api/events?dev=&limit=` | Recent history (newest first, max 500) |
| POST | `/api/devices/:id/cmd` | Body `{"led":true}` → publishes to `dev/:id/cmd` |
| WS | `/ws` | `snapshot` on connect, then `device` / `event` pushes |

## Database (SQLite, `data/iot.db`)

| Table | Holds |
|---|---|
| `devices` | Current state per board: led, source, boot, seq, rssi, fw, online |
| `events` | History: button clicks, LED changes, with source |
| `commands` | Audit trail: every command sent, by whom, when |

## Design notes

- **Device messages are untrusted input.** Every payload is schema-validated
  (zod); malformed messages are logged and dropped, never stored.
- **Topic/payload must agree.** A message on `dev/a/state` claiming `"dev":"b"`
  is rejected, so one device cannot speak for another. (Phase 8's broker ACL
  enforces this at the broker too.)
- **Stale messages are dropped** using the device's `(boot, seq)` ordering key,
  so a delayed or replayed retained message cannot overwrite newer truth.
- **Commands return 202, not 200.** The UI updates only when the device
  publishes its new state. The dashboard never shows a state it assumed.
- **Every command is written to `commands`** with an actor. It is `"lan"` now
  and becomes the authenticated user in Phase 9.
- **Fail fast on bad config:** a missing or empty `MQTT_PASS` stops startup
  rather than silently connecting anonymously.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `Invalid configuration` at start | A field in `.env` is missing or empty |
| MQTT connects then `Connection refused: Not authorized` | Wrong `backend` password |
| `better-sqlite3` install fails | `sudo apt install -y build-essential python3`, then `npm rebuild` |
| `/api/devices` empty | Boards haven't published since the backend started; press a button or restart a board |
| Windows browser can't open `localhost:8080` | WSL not in mirrored mode, or the backend isn't running |
