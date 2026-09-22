// Dashboard. Shows only device-confirmed state: a toggle stays "pending" until
// the board publishes its new state, so the UI never lies about the hardware.

const devicesEl = document.getElementById("devices");
const devicesEmptyEl = document.getElementById("devices-empty");
const eventsEl = document.getElementById("events");
const filterEl = document.getElementById("filter");
const connEl = document.getElementById("conn");
const connTextEl = document.getElementById("conn-text");

const PENDING_TIMEOUT_MS = 6000;
const MAX_EVENTS = 200;

/** @type {Map<string, object>} */
const devices = new Map();
/** @type {Map<string, {want: boolean, timer: number}>} */
const pending = new Map();
/** @type {object[]} */
let events = [];

let socket = null;
let backoff = 1000;

function setConn(state, text) {
  connEl.className = `conn ${state}`;
  connTextEl.textContent = text;
}

function relTime(ts) {
  if (!ts) return "never";
  const s = Math.round((Date.now() - ts) / 1000);
  if (s < 60) return `${s}s ago`;
  if (s < 3600) return `${Math.round(s / 60)}m ago`;
  if (s < 86400) return `${Math.round(s / 3600)}h ago`;
  return new Date(ts).toLocaleDateString();
}

function clockTime(ts) {
  return new Date(ts).toLocaleTimeString([], { hour12: false });
}

function renderDevices() {
  devicesEmptyEl.hidden = devices.size > 0;
  const ids = [...devices.keys()].sort();

  for (const id of ids) {
    const d = devices.get(id);
    let card = document.getElementById(`dev-${id}`);
    if (!card) {
      card = buildCard(id);
      devicesEl.append(card);
    }
    updateCard(card, d);
  }

  for (const card of [...devicesEl.querySelectorAll(".card")]) {
    if (!devices.has(card.dataset.id)) card.remove();
  }
  syncFilterOptions(ids);
}

function buildCard(id) {
  const card = document.createElement("article");
  card.className = "card";
  card.id = `dev-${id}`;
  card.dataset.id = id;

  const top = document.createElement("div");
  top.className = "card-top";
  const name = document.createElement("span");
  name.className = "name";
  name.textContent = id; // textContent, never innerHTML: device ids are untrusted
  const badge = document.createElement("span");
  badge.className = "badge";
  top.append(name, badge);

  const control = document.createElement("div");
  control.className = "control";
  const state = document.createElement("div");
  state.className = "state";
  const lamp = document.createElement("span");
  lamp.className = "lamp";
  const stateText = document.createElement("span");
  const pendingText = document.createElement("span");
  pendingText.className = "pending";
  state.append(lamp, stateText, pendingText);

  const button = document.createElement("button");
  button.textContent = "Toggle";
  button.addEventListener("click", () => sendCommand(id));
  control.append(state, button);

  const meta = document.createElement("div");
  meta.className = "meta";

  card.append(top, control, meta);
  return card;
}

function updateCard(card, d) {
  const online = d.online === 1;
  const led = d.led === 1;
  const p = pending.get(d.id);

  card.classList.toggle("offline", !online);
  card.querySelector(".badge").textContent = online ? "online" : "offline";
  card.querySelector(".badge").className = `badge ${online ? "online" : "offline"}`;
  card.querySelector(".lamp").className = `lamp ${led ? "on" : ""}`;
  card.querySelector(".state span:nth-child(2)").textContent = led ? "LED on" : "LED off";
  card.querySelector(".pending").textContent = p ? `waiting for ${p.want ? "on" : "off"}…` : "";
  card.querySelector("button").disabled = !online || Boolean(p);

  const meta = card.querySelector(".meta");
  meta.replaceChildren(
    line(`last change: ${d.src ?? "—"}`),
    line(`seen: ${relTime(d.last_seen)}`),
    line(`rssi: ${d.rssi ?? "—"} dBm · fw ${d.fw ?? "—"} · boot ${d.boot ?? "—"}`),
  );
}

function line(text) {
  const el = document.createElement("span");
  el.textContent = text;
  return el;
}

function syncFilterOptions(ids) {
  const current = filterEl.value;
  const wanted = ["", ...ids].join("|");
  if (filterEl.dataset.ids === wanted) return;
  filterEl.dataset.ids = wanted;
  filterEl.replaceChildren();
  for (const id of ["", ...ids]) {
    const opt = document.createElement("option");
    opt.value = id;
    opt.textContent = id || "all";
    filterEl.append(opt);
  }
  filterEl.value = current;
}

function renderEvents() {
  const filter = filterEl.value;
  const shown = events.filter((e) => !filter || e.dev === filter).slice(0, MAX_EVENTS);
  eventsEl.replaceChildren(
    ...shown.map((e) => {
      const li = document.createElement("li");
      const time = document.createElement("span");
      time.className = "time";
      time.textContent = clockTime(e.ts);
      const dev = document.createElement("span");
      dev.className = "dev";
      dev.textContent = e.dev;
      const what = document.createElement("span");
      what.textContent = `${e.type} ${e.action}`;
      const src = document.createElement("span");
      src.className = `src src-${e.src}`;
      src.textContent = e.src === "button" ? "manual" : e.src;
      li.append(time, dev, what, src);
      return li;
    }),
  );
}

async function sendCommand(id) {
  const d = devices.get(id);
  if (!d) return;
  const want = d.led !== 1;

  const timer = window.setTimeout(() => {
    pending.delete(id);
    renderDevices();
  }, PENDING_TIMEOUT_MS);
  pending.set(id, { want, timer });
  renderDevices();

  try {
    const res = await fetch(`/api/devices/${encodeURIComponent(id)}/cmd`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ led: want }),
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
  } catch (err) {
    clearTimeout(timer);
    pending.delete(id);
    renderDevices();
    console.error("Command failed", err);
  }
}

function applyDevice(d) {
  devices.set(d.id, d);
  const p = pending.get(d.id);
  if (p && d.led === (p.want ? 1 : 0)) {
    clearTimeout(p.timer); // device confirmed: stop waiting
    pending.delete(d.id);
  }
  renderDevices();
}

function connect() {
  const proto = location.protocol === "https:" ? "wss" : "ws";
  socket = new WebSocket(`${proto}://${location.host}/ws`);

  socket.addEventListener("open", () => {
    backoff = 1000;
    setConn("up", "live");
  });

  socket.addEventListener("message", (ev) => {
    let msg;
    try {
      msg = JSON.parse(ev.data);
    } catch {
      return;
    }
    if (msg.type === "snapshot") {
      devices.clear();
      for (const d of msg.devices) devices.set(d.id, d);
      events = msg.events;
      renderDevices();
      renderEvents();
    } else if (msg.type === "device") {
      applyDevice(msg.device);
    } else if (msg.type === "event") {
      events = [msg.event, ...events].slice(0, MAX_EVENTS);
      renderEvents();
    }
  });

  socket.addEventListener("close", () => {
    setConn("down", "reconnecting…");
    setTimeout(connect, backoff);
    backoff = Math.min(backoff * 2, 15000);
  });

  socket.addEventListener("error", () => socket.close());
}

filterEl.addEventListener("change", renderEvents);
setInterval(renderDevices, 10000); // keep "seen: Xs ago" fresh
connect();
