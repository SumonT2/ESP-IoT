# Phase 5 — Dashboard (LAN only)

Live web UI served by the backend itself: one card per board, a toggle, and a
running activity feed that shows manual button presses and remote commands
side by side.

## Run

```bash
cd ~/ESP-IoT/server/backend
git pull
npm install          # picks up @fastify/static
npm run dev
```

Open from Windows: <http://localhost:8080>

(Still loopback-only. Phase 6 puts it behind Cloudflare Access before anything
outside this machine can reach it.)

## What you should see

- A card per board: online/offline, LED state, signal, firmware, boot count
- **Toggle** button → shows `waiting for on…` until the board confirms, then the
  lamp turns green
- Press the board's physical button → the card updates within a second and the
  activity feed shows a **manual** entry
- Pull a board's power → its card goes offline (Last Will), and the toggle is
  disabled
- Stop the backend → the header shows `reconnecting…`; start it again → `live`

## Checks

- [ ] All 3 boards appear; states match the real LEDs
- [ ] Toggle from the browser changes the LED; entry shows `remote`
- [ ] Physical press appears in the feed as `manual`
- [ ] Two browser tabs stay in sync with each other
- [ ] Unplug a board → offline within ~45 s
- [ ] Refresh the page → state and recent history are still correct

## Design notes

- **No optimistic UI.** A toggle shows `waiting…` and only turns green when the
  device publishes its new state, so the dashboard can never claim an LED is on
  when it isn't. If no confirmation arrives in 6 s, the pending mark clears.
- **Same origin for API, WebSocket and page:** no CORS, one port to protect.
- **Device text is inserted with `textContent`, never `innerHTML`**, so a device
  id or event value cannot inject HTML into the page.
- **Security headers** on every response: CSP limiting scripts and styles to
  this origin, `X-Frame-Options: DENY` (no clickjacking), `nosniff`,
  `no-referrer`.
- **No build step, no framework, no CDN.** Fewer dependencies to keep patched,
  and the CSP can stay strict. Stage C can replace this UI without touching the
  API.
- **WebSocket reconnects with backoff**, and reloads a full snapshot on connect,
  so a dropped link cannot leave a stale screen.
