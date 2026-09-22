# Phase 2 — Server prep (WSL2 Ubuntu on T2-PC)

Goal: Mosquitto MQTT broker running in WSL Ubuntu, reachable from the LAN
(ESP boards) on `192.168.68.106:1883`, password-protected, staying up while
Windows is on.

| Item | Value |
|---|---|
| Host | T2-PC, Windows 11 build 26200, Ethernet `192.168.68.106`, gateway `192.168.68.1` |
| WSL | 2.7.14, distro `Ubuntu-26.04`, systemd enabled |
| Broker | Mosquitto, port 1883, no anonymous access |

---

## Part A — Windows (PowerShell **as Administrator**)

### A1. WSL config: mirrored networking, never idle-shutdown

Create or replace `%UserProfile%\.wslconfig` (it does not exist yet):

```powershell
@'
[wsl2]
networkingMode=mirrored
firewall=true

[general]
instanceIdleTimeout=-1
'@ | Set-Content -Encoding ascii "$env:USERPROFILE\.wslconfig"

wsl --shutdown
```

- `mirrored` — WSL shares the Windows network interfaces, so services in
  Ubuntu are reachable at `192.168.68.106`.
- `firewall=true` — Windows Hyper-V firewall filters WSL traffic (we rely on it).
- `instanceIdleTimeout=-1` — distro no longer stops ~15 s after the last terminal closes.

Wait ~10 s, then verify from PowerShell:

```powershell
wsl -d Ubuntu-26.04 -- hostname -I
```

Must now include `192.168.68.106` (no longer `172.31.x.x`).

### A2. Find your LAN subnet

```powershell
(Get-NetIPAddress -InterfaceAlias Ethernet -AddressFamily IPv4).PrefixLength
```

| Output | Subnet to use below |
|---|---|
| `24` | `192.168.68.0/24` |
| `22` | `192.168.68.0/22` (TP-Link Deco default) |

### A3. Firewall: allow MQTT from the LAN only

Replace `<SUBNET>` with the value from A2:

```powershell
New-NetFirewallHyperVRule -Name "WSL-MQTT-LAN" -DisplayName "WSL Mosquitto 1883 (LAN only)" `
  -Direction Inbound -VMCreatorId '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}' `
  -Protocol TCP -LocalPorts 1883 -RemoteAddresses <SUBNET> -Action Allow
```

`{40E0AC32-...}` is the fixed ID Windows uses for WSL. Check the rule:

```powershell
Get-NetFirewallHyperVRule -Name "WSL-MQTT-LAN" | Format-List DisplayName,Enabled,Action,LocalPorts,RemoteAddresses
```

### A4. Keep the PC awake (on mains power)

```powershell
powercfg /change standby-timeout-ac 0
powercfg /change hibernate-timeout-ac 0
```

Screen may still turn off; only sleep is disabled.

### A5. Start WSL automatically at logon

```powershell
schtasks /create /tn "WSL IoT server" /sc onlogon /tr "wsl.exe -d Ubuntu-26.04 --exec /bin/true" /rl limited /f
```

With `instanceIdleTimeout=-1`, the distro (and Mosquitto via systemd) keeps
running after this starts it.

### A6. Fixed IP (router)

Reserve `192.168.68.106` for T2-PC's Ethernet adapter in the router's DHCP
settings (Deco app: **More → Advanced → Address Reservation**). ESP boards will
be configured with this IP.

---

## Part B — Ubuntu (WSL terminal)

### B1. Update and install

```bash
sudo apt update && sudo apt full-upgrade -y
sudo apt install -y mosquitto mosquitto-clients
```

### B2. Broker config (password required, LAN listener)

```bash
sudo tee /etc/mosquitto/conf.d/lan.conf >/dev/null <<'EOF'
listener 1883
allow_anonymous false
password_file /etc/mosquitto/passwd
EOF
```

(Same content as `server/mosquitto/conf.d/lan.conf` in this repo.)

### B3. Create MQTT users — one per device + backend + admin

Password file readable only by the broker:

```bash
sudo install -m 600 -o mosquitto -g mosquitto /dev/null /etc/mosquitto/passwd
```

Generate strong random passwords and add the users:

```bash
for u in admin backend esp32c6 esp32c3 esp8266; do
  pw=$(openssl rand -base64 18)
  sudo mosquitto_passwd -b /etc/mosquitto/passwd "$u" "$pw"
  printf '%-8s %s\n' "$u" "$pw"
done
```

> ⚠️ **Copy the printed passwords into a password manager now.** They are
> shown only once (stored hashed). Never commit them to git, never paste them
> into chat. The device passwords go into each board's git-ignored `secrets.h`
> in Phase 3. Lost one? Re-run `sudo mosquitto_passwd -b ...` for that user.

### B4. Start the broker

```bash
sudo systemctl enable --now mosquitto
sudo systemctl restart mosquitto
systemctl is-active mosquitto
sudo ss -tlnp | grep 1883
```

Expect `active` and a listener on `0.0.0.0:1883`.

### B5. Test (local)

Load the admin password into a variable without echoing it or saving it to
shell history:

```bash
read -rsp "admin MQTT password: " MQTT_PW; echo
```

Terminal 1 — subscribe:

```bash
mosquitto_sub -h localhost -u admin -P "$MQTT_PW" -t 'test/#' -v
```

Terminal 2 (run the same `read` first) — publish:

```bash
mosquitto_pub -h localhost -u admin -P "$MQTT_PW" -t test/hello -m "hi from WSL"
```

Terminal 1 shows `test/hello hi from WSL`. ✅

Negative test — anonymous must be refused:

```bash
mosquitto_pub -h localhost -t test/hello -m nope
```

Expect `Connection error: Connection Refused: not authorised.` ✅

### B6. Survives restart?

In PowerShell: `wsl --shutdown`, wait 10 s, then run the logon task
(`schtasks /run /tn "WSL IoT server"`), close all terminals, wait 1 min:

```powershell
wsl -l -v                                  # Ubuntu-26.04 should be Running
wsl -d Ubuntu-26.04 -- systemctl is-active mosquitto   # active
```

---

## Done when

- [ ] `hostname -I` in WSL shows `192.168.68.106`
- [ ] Hyper-V firewall rule `WSL-MQTT-LAN` exists, limited to LAN subnet
- [ ] Mosquitto `active`, listening on 1883
- [ ] Authenticated pub/sub works; anonymous refused
- [ ] Distro stays Running with all terminals closed
- [ ] Passwords saved in a password manager, nowhere else

The LAN test from a real device happens in Phase 3, when the boards connect.

## Troubleshooting

| Symptom | Check |
|---|---|
| `hostname -I` still `172.x` | `.wslconfig` saved in `C:\Users\<you>\`? Ran `wsl --shutdown`? |
| Mosquitto fails to start | `sudo journalctl -u mosquitto -n 30` |
| Board can't connect (Phase 3) | `Get-NetFirewallHyperVVMSetting -PolicyStore ActiveStore -Name '{40E0AC32-46A5-438A-A0B2-2B479E8F2E90}'` and rule subnet |
| Distro stops by itself | `[general]` section in `.wslconfig`, WSL version ≥ 2.x |
