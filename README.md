# ESP32 IoT — Secure Remote + Manual Control

Control devices remotely from a web dashboard **and** manually (physical buttons),
with the dashboard always showing the real, device-confirmed state and a history
of who/what changed it.

## Hardware

| Board | Chip | Role | Build env |
|---|---|---|---|
| ESP32-C6 | ESP32-C6 (RISC-V, Wi-Fi 6) | Network node | `esp32c6` |
| ESP32-C3 Super Mini | ESP32-C3 (RISC-V, Wi-Fi 4) | Network node | `esp32c3` |
| NodeMCU | ESP8266 (ESP-12E) | Network node (limited TLS RAM) | `esp8266` |
| Arduino Uno / Nano / Mini / Micro | AVR | I/O expander over UART (no network) | `uno`, `nano`, ... |

### Verified board facts (Phase 0)

| Board | Chip | Flash | Free heap | Onboard LED | USB serial |
|---|---|---|---|---|---|
| ESP32-C6 | C6 rev 0, 1 core @160 MHz | 8 MB | ~430 KB | GPIO8, WS2812 RGB | UART bridge |
| ESP32-C3 Super Mini | C3 rev 4, 1 core @160 MHz | 4 MB | ~290 KB | GPIO8, active LOW | Native USB (CDC on boot) |
| NodeMCU | ESP8266 @80 MHz | 4 MB | ~52 KB | GPIO2, active LOW | CH340 (driver needed) |

## Decisions

- **Server:** on-premises Ubuntu server, same LAN as the devices.
- **Remote access:** Cloudflare Tunnel (`cloudflared`, outbound only, no router
  port forwarding) protected by **Cloudflare Access** (login gate).
- **Devices talk to the server over the LAN only.** MQTT is never tunnelled or
  exposed; only the web dashboard goes through the tunnel.
- **Local-first:** physical buttons work even when Wi-Fi/server is down; state is
  re-synced on reconnect.
- **Source of truth for UI = device-reported state**, never assumed state.

## Architecture

```
 Browser (anywhere)
    │ HTTPS
    ▼
 Cloudflare edge ── Cloudflare Access (email OTP / Google login, allow-list)
    │ tunnel (outbound connection from server, no open ports)
    ▼
 ┌──────────── On-prem Ubuntu server (LAN) ────────────┐
 │ cloudflared  : tunnel → only the dashboard port     │
 │ Backend API  : commands, audit log (app auth later) │
 │ WebSocket    : live state push to browsers          │
 │ Mosquitto    : MQTT broker, LAN only (1883 → 8883)  │
 │ Database     : device state + event history         │
 └──────────────────────▲───────────────────────────────┘
                        │ MQTT over Wi-Fi/LAN
          ┌─────────────┼──────────────┐
      ESP32-C6     ESP32-C3 SM     ESP8266 NodeMCU
          │ UART
      Arduino (extra I/O)
```

### Topics (per device, enforced by broker ACL in Stage B)

| Topic | Direction | Retained | Example |
|---|---|---|---|
| `dev/<id>/state` | device → server | yes | `{"led":1,"btn":0,"src":"button","seq":42}` |
| `dev/<id>/event` | device → server | no | `{"type":"button","action":"press","ts":...}` |
| `dev/<id>/cmd` | server → device | no | `{"set":{"led":0},"nonce":"...","ts":...}` |
| `dev/<id>/status` | device (LWT) | yes | `online` / `offline` |

### Manual vs remote flow

1. Button pressed → device toggles LED immediately → publishes `state` (`src:"button"`) + `event`.
2. Server stores it, logs it, pushes to browsers via WebSocket.
3. Dashboard click → server validates user → publishes `cmd` → device applies →
   publishes `state` (`src:"remote"`) → UI updates only on that confirmation.

## Security layers

1. **Perimeter:** no inbound ports on router. Only `cloudflared` reaches out.
2. **Remote login:** Cloudflare Access policy (explicit email allow-list) in front
   of every tunnelled hostname. Created **before** the hostname is routed.
3. **Server:** SSH keys only, `ufw` default-deny (MQTT allowed from LAN subnet only),
   `fail2ban`, unattended-upgrades.
4. **MQTT:** username/password now → TLS + per-device client certs (mTLS) + ACL later.
5. **App:** own auth layer (Argon2 + TOTP/passkey) behind Access (defence in depth),
   verify Cloudflare Access JWT, command allow-list, nonce + timestamp anti-replay.
6. **Firmware:** secrets in NVS (encrypted later), signed OTA.
7. **Optional, irreversible:** Secure Boot v2 + Flash Encryption (ESP32-C6/C3 only).

> ⚠️ Step 7 permanently burns eFuses. Done last, on one test board, with explicit checklist.

## Roadmap

### Stage A — Build it

| # | Phase | Status |
|---|---|---|
| 0 | Toolchain + board check (identify chips, blink) | ✅ done |
| 1 | Button + LED firmware, debounce, local state machine | ▶ next |
| 2 | Server prep: static IP, SSH keys, ufw; Mosquitto on LAN 1883 | |
| 3 | Device MQTT client (state/cmd/LWT, reconnect) per board | |
| 4 | Backend API + DB + WebSocket | |
| 5 | Web dashboard (live state, manual/remote history) — LAN only | |
| 6 | **Remote gate:** Cloudflare Access policy, then cloudflared tunnel | |
| 7 | Arduino I/O expanders over UART | |

Stage A ground rules:
- No router port forwarding, ever.
- Never use a "quick tunnel" (`trycloudflare.com`) — it has no Access gate.
- Tunnel publishes only the dashboard; never MQTT, SSH, or database ports.
- Wi-Fi/MQTT credentials only in git-ignored `secrets.h` / `.env`.
- MQTT connection code isolated behind one module so TLS is a config switch later.

### Stage B — Harden it

| # | Phase | Status |
|---|---|---|
| 8 | Private CA + Mosquitto TLS 8883, mTLS, ACL; switch devices over | |
| 9 | App auth (Argon2 + TOTP/passkey) + Access JWT validation + headers | |
| 10 | Server hardening pass (fail2ban, auto-updates, backups, audit) | |
| 11 | Signed OTA updates | |
| 12 | Secure Boot + Flash Encryption (optional, irreversible) | |

## Layout

```
firmware/
  00-board-check/   Phase 0: identify chip + blink
  01-button-led/    Phase 1: debounced button, LED, persisted state, serial console
server/             (Phase 2+)
docs/               per-phase guides
```
