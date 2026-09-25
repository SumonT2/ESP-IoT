# Phase 6 — Remote access (Cloudflare Tunnel + Access)

Reach the dashboard from anywhere, with **no open ports** on your router and a
login gate in front of it.

```
phone/laptop ──HTTPS──> Cloudflare edge ──[Access login]──> tunnel ──> localhost:8080 (backend)
```

Replace `iot.example.com` below with your own hostname.

> ⚠️ **Order matters.** Create the Access application **before** routing the
> DNS name. If the hostname resolves before the policy exists, the dashboard is
> open to the entire internet until you finish. Anyone who finds it can toggle
> your hardware.

---

## 1. Install cloudflared (WSL Ubuntu)

```bash
sudo mkdir -p --mode=0755 /usr/share/keyrings
curl -fsSL https://pkg.cloudflare.com/cloudflare-main.gpg | sudo tee /usr/share/keyrings/cloudflare-main.gpg >/dev/null
echo "deb [signed-by=/usr/share/keyrings/cloudflare-main.gpg] https://pkg.cloudflare.com/cloudflared any main" | sudo tee /etc/apt/sources.list.d/cloudflared.list
sudo apt update && sudo apt install -y cloudflared
cloudflared --version
```

## 2. Log in and create the tunnel

```bash
cloudflared tunnel login
```

It prints a URL. Open it in your Windows browser, pick your domain, authorize.
A certificate lands in `~/.cloudflared/cert.pem`.

```bash
cloudflared tunnel create iot
cloudflared tunnel list
```

Note the tunnel **UUID**. Its credentials file is `~/.cloudflared/<UUID>.json`.

> Those two files are keys to your tunnel. Never commit or share them.

## 3. Tunnel config

```bash
sudo mkdir -p /etc/cloudflared
sudo tee /etc/cloudflared/config.yml >/dev/null <<'EOF'
tunnel: iot
credentials-file: /etc/cloudflared/iot.json

ingress:
  - hostname: iot.example.com
    service: http://localhost:8080
  # Anything else gets nothing: the tunnel exposes exactly one service.
  - service: http_status:404
EOF

sudo cp ~/.cloudflared/<UUID>.json /etc/cloudflared/iot.json
sudo chmod 600 /etc/cloudflared/iot.json
cloudflared tunnel --config /etc/cloudflared/config.yml ingress validate
```

## 4. Create the Access application FIRST

In <https://one.dash.cloudflare.com> → **Access → Applications → Add an
application → Self-hosted**:

| Field | Value |
|---|---|
| Application name | IoT Dashboard |
| Session duration | 24 hours |
| Public hostname | `iot.example.com` |

Then **Add policy**:

| Field | Value |
|---|---|
| Policy name | Owner only |
| Action | **Allow** |
| Include | **Emails** → your email address |

Save. Anything not matching a policy is denied by default, so only that email
gets in. Add more addresses later for family members.

> Cloudflare emails a one-time code at each login. If you prefer, add a Google
> or GitHub identity provider under **Settings → Authentication** and include
> that instead.

## 5. Now route DNS

```bash
cloudflared tunnel route dns iot iot.example.com
```

This creates a proxied CNAME to the tunnel. Because step 4 is already in place,
the hostname is gated from its first second of existence.

## 6. Test before making it permanent

```bash
cloudflared tunnel --config /etc/cloudflared/config.yml run
```

From your phone (mobile data, not Wi-Fi), open `https://iot.example.com`:

1. Cloudflare asks for your email, then a one-time code
2. The dashboard loads, live
3. Toggle a board and watch the physical LED change

Stop it with `Ctrl+C` once that works.

## 7. Run it as a service

```bash
sudo cloudflared --config /etc/cloudflared/config.yml service install
sudo systemctl enable --now cloudflared
systemctl status cloudflared --no-pager
journalctl -u cloudflared -f
```

The backend must run too (`iot-backend` service from Phase 4), and WSL must be
up — the logon task from Phase 2 handles that.

---

## Checks

- [ ] `https://iot.example.com` in a private window asks for login, **before** showing anything
- [ ] A different email address is refused
- [ ] Dashboard works on mobile data: live states, toggle, history
- [ ] Physical button press appears on the phone within ~1 s
- [ ] `curl -I https://iot.example.com/api/devices` returns a redirect/403, **not** device JSON
- [ ] Router still has **no** port forwarding
- [ ] Reboot Windows → log in → dashboard reachable again without touching anything

## Why this is safe

- **No inbound ports.** `cloudflared` makes an outbound connection; your router
  stays closed.
- **Access runs at Cloudflare's edge**, so unauthenticated requests never reach
  your machine.
- **The backend still binds to `127.0.0.1`**, so only the tunnel can reach it —
  not the LAN, not the internet.
- **One hostname, one service.** The catch-all `404` rule means a
  misconfiguration can't expose Mosquitto, SSH or anything else.
- **Cloudflare terminates TLS**, so they can see this traffic. Fine for LED
  states; worth knowing before adding cameras or anything sensitive.

## Still missing (Phase 9)

Access is the *only* gate right now. The backend itself does not yet check who
is calling: anything that reaches it is trusted. Phase 9 adds app-level login
and verifies the `Cf-Access-Jwt-Assertion` header Cloudflare attaches, so a
request that somehow bypasses the edge is still rejected.

## Troubleshooting

| Symptom | Check |
|---|---|
| `502 Bad Gateway` | Backend not running: `systemctl status iot-backend`, or wrong port in `config.yml` |
| Login loop | Clock skew in WSL, or third-party cookies blocked in the browser |
| Dashboard loads but stays `reconnecting…` | WebSocket blocked: the Access app must be the **self-hosted** type; check `journalctl -u cloudflared` |
| Works on Wi-Fi, not mobile data | You were hitting the LAN address, not the tunnel — clear the cached page |
| `Tunnel not found` | `cloudflared tunnel list` — name in `config.yml` must match |
