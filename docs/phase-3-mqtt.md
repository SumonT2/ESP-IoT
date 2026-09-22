# Phase 3 — Boards on MQTT

Button press on a board shows up on the server instantly; a message from the
server changes the LED. Still LAN only, still plain MQTT (TLS is Phase 8).

## 1. Fill in secrets (once per checkout)

```bash
cd firmware/03-mqtt-node/src
cp secrets.h.example secrets.h
```

Edit `secrets.h`: Wi-Fi name/password and the three MQTT passwords created in
Phase 2 step B3. `secrets.h` is git-ignored — never commit it, never paste it
into chat.

> The boards need **2.4 GHz Wi-Fi**. None of them support 5 GHz. If your router
> merges both bands under one name, that usually still works, but a separate
> 2.4 GHz SSID is more reliable.

## 2. Flash each board

The build environment picks the right MQTT account automatically
(`esp32c6` / `esp32c3` / `esp8266`).

```
pio run -e esp32c6 -t upload -t monitor
pio run -e esp32c3 -t upload -t monitor
pio run -e esp8266 -t upload -t monitor
```

Expected serial output:

```
[I] Wi-Fi up: ip=192.168.68.x rssi=-52 dBm
[I] MQTT connecting to 192.168.68.106:1883 as 'esp32c3'
[I] MQTT connected, subscribed to dev/esp32c3/cmd
```

## 3. Watch everything from the server (WSL terminal)

```bash
read -rsp "admin MQTT password: " MQTT_PW; echo
mosquitto_sub -h localhost -u admin -P "$MQTT_PW" -t 'dev/#' -v
```

Press a board's button: `dev/<id>/event` and `dev/<id>/state` appear at once.

## 4. Control a board from the server

```bash
mosquitto_pub -h localhost -u admin -P "$MQTT_PW" -t dev/esp32c3/cmd -m '{"set":{"led":1}}'
```

LED turns on, and the board publishes `"src":"remote"`. The LED changing is
what proves it: the dashboard in Phase 5 will trust only these confirmations.

## Topics

| Topic | Who publishes | Retained |
|---|---|---|
| `dev/<id>/state` | device | yes — new subscribers get the last state immediately |
| `dev/<id>/event` | device | no |
| `dev/<id>/cmd` | server | no |
| `dev/<id>/status` | device / broker (Last Will) | yes — `online` / `offline` |

## Checks

- [ ] All 3 boards connect; `mosquitto_sub -t 'dev/#'` shows 3 `status online`
- [ ] Button press → `event` + `state` with `"src":"button"`
- [ ] `cmd` publish → LED changes, `state` with `"src":"remote"`
- [ ] Bad command (`-m 'garbage'`, or `{"set":{"led":"x"}}`) → ignored, warning logged, LED unchanged
- [ ] Command to another device's topic → only that device reacts
- [ ] Unplug a board → after ~45 s its `status` becomes `offline` (Last Will)
- [ ] Stop Mosquitto (`sudo systemctl stop mosquitto`) → **button still toggles the LED**; restart → board reconnects and republishes its state
- [ ] Reboot a board → `boot` counter increases, LED restores

## Design notes

- **Local-first:** `loop()` handles the button before the network, and no
  network call blocks. A dead server never blocks manual control.
- **Backoff:** reconnects slow down to 30 s, so a down broker is not hammered.
- **Resync:** on every reconnect the device republishes its retained state, so
  the server can never be stuck with a stale value.
- **Command allow-list:** only `{"set":{"led":<bool>}}` is accepted; oversized,
  malformed or unknown commands are ignored and logged.
- **QoS 0 for publishes** (PubSubClient limit): an event can be lost on a bad
  link. State is retained, so the *current* value is always recoverable. Phase 4
  server-side will treat `state` as truth and `event` as best-effort history.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `MQTT connect failed, rc=5` | Wrong username/password, or that user was not created |
| `MQTT connect failed, rc=-2` | Can't reach the broker: firewall rule subnet, WSL not in mirrored mode, or Mosquitto stopped |
| Wi-Fi never connects | 5 GHz-only SSID, wrong password, or weak signal (`rssi` worse than -80 dBm) |
| Board connects then drops every ~30 s | Two boards sharing one MQTT client id / account |
| ESP8266 resets repeatedly | Power: use a good USB cable/port; Wi-Fi peaks draw ~300 mA |
