// ═══════════════════════════════════════════════════════════
//  Formula Manipal — Telemetry Dashboard  v3
//  Binary 79-byte packets + text/CSV/JSON parser
//  RAF render loop — decoupled from receive path for max Hz
// ═══════════════════════════════════════════════════════════
"use strict";

const PACKET_SIZE = 79;
const START_BYTE = 0xaa;
const END_BYTE = 0x55;
const MAX_LOG_LINES = 80;
const CHART_POINTS = 300; // points kept in memory
const CHART_VISIBLE = 200; // points rendered (last 200)

// ── State ─────────────────────────────────────────────────
let port = null,
  reader = null;
let isConnected = false,
  demoMode = false;
let ws = null,
  isWsConnected = false;
let demoTimer = null,
  demoT = 0;
let packetCount = 0,
  rateCounter = 0,
  rateLastTime = Date.now();

let sessionCsvData = [];
const CSV_HEADER =
  "Timestamp,BMS_Total_V,BMS_Avg_Temp_C,BMS_Current_A,BMS_Low_Cell_V,Motor_Speed_RPM,Seg0_V,Seg1_V,Seg2_V,Seg3_V,Seg4_V,VSM,PosX,PosY,PosZ,WheelRPM,Speed_kmh,Yaw_deg,Pitch_deg,Roll_deg";

// Position
let positionHistory = [],
  lastPosition = { x: 0, y: 0, z: 0 };
let totalDistance = 0,
  lastUpdateTime = 0;
let originSet = false,
  originX = 0,
  originY = 0,
  originZ = 0;

// Binary buffer
let rxBuf = new Uint8Array(4096),
  rxLen = 0;
let lineBuf = "";

// ── RAF-batched log ───────────────────────────────────────
let logQueue = []; // { msg, type }
let rafScheduled = false;

// ── Chart data buffers ────────────────────────────────────
// Each buffer is a ring of { t, v } objects (t = ms timestamp)
const BUF = CHART_POINTS;
const bufs = {
  motorRPM: [],
  wheelSpeed: [],
  packVoltage: [],
  packCurrent: [],
  packTemp: [],
  lowCellV: [],
  yaw: [],
  pitch: [],
  roll: [],
  vehicleSpeed: [],
};

// Pending values to push to charts on next RAF frame
let pendingChartData = null;

// ── VSM State Names ───────────────────────────────────────
const VSM_NAMES = {
  0: "VSM START",
  1: "PRE-CHARGE INIT",
  2: "PRE-CHARGE ACTIVE",
  3: "PRE-CHARGE COMPLETE",
  4: "VSM WAIT",
  5: "VSM READY",
  6: "MOTOR RUNNING",
  7: "BLINK",
  14: "SHUTDOWN",
  15: "RECYCLE",
};

// ── DOM helper ────────────────────────────────────────────
function $(id) {
  return document.getElementById(id);
}

// ═══════════════════════════════════════════════════════════
//  CHART SETUP  — F1 pitwall style
// ═══════════════════════════════════════════════════════════
Chart.defaults.color = "#666";
Chart.defaults.borderColor = "#151515";

// Plugin: draw current value label at right edge of each dataset
const currentValuePlugin = {
  id: "currentValue",
  afterDatasetsDraw(chart) {
    const ctx = chart.ctx;
    chart.data.datasets.forEach((ds, i) => {
      const meta = chart.getDatasetMeta(i);
      if (!meta.visible) return;
      // Find last non-null point
      let lastPt = null;
      for (let j = ds.data.length - 1; j >= 0; j--) {
        if (ds.data[j] !== null && isFinite(ds.data[j])) {
          lastPt = meta.data[j];
          break;
        }
      }
      if (!lastPt) return;
      const val = ds.data.filter((v) => v !== null && isFinite(v));
      if (!val.length) return;
      const latest = val[val.length - 1];
      const label =
        Math.abs(latest) >= 100
          ? latest.toFixed(0)
          : Math.abs(latest) >= 10
          ? latest.toFixed(1)
          : latest.toFixed(2);
      ctx.save();
      ctx.font = `bold 9px 'Share Tech Mono', monospace`;
      ctx.fillStyle = ds.borderColor;
      ctx.textAlign = "right";
      ctx.shadowColor = ds.borderColor;
      ctx.shadowBlur = 4;
      ctx.fillText(label, chart.chartArea.right - 4, lastPt.y - 4);
      ctx.restore();
    });
  },
};
Chart.register(currentValuePlugin);

function mkDataset(label, color, data, yAxisID = "y") {
  return {
    label,
    data,
    yAxisID,
    borderColor: color,
    borderWidth: 2,
    pointRadius: 0,
    tension: 0.2,
    fill: false,
    backgroundColor: "transparent",
  };
}

function mkChart(id, datasets, yMin, yMax, extraScales = {}) {
  const ctx = document.getElementById(id);
  if (!ctx) return null;
  return new Chart(ctx, {
    type: "line",
    data: { labels: Array(CHART_VISIBLE).fill(""), datasets },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: false,
      interaction: { mode: "index", intersect: false },
      plugins: {
        legend: { display: false },
        tooltip: { enabled: false },
      },
      scales: {
        x: { display: false },
        y: {
          display: true,
          min: yMin,
          max: yMax,
          position: "left",
          grid: { color: "#151515", drawBorder: false },
          border: { color: "#222", dash: [2, 4] },
          ticks: {
            color: "#555",
            font: { family: "'Share Tech Mono'", size: 10 },
            maxTicksLimit: 4,
            callback: (v) =>
              Math.abs(v) >= 100
                ? v.toFixed(0)
                : Math.abs(v) >= 10
                ? v.toFixed(1)
                : v.toFixed(2),
          },
        },
        ...extraScales,
      },
    },
  });
}

const charts = {
  powertrain: mkChart(
    "chart-powertrain",
    [
      mkDataset("Motor RPM", "#ff2020", Array(CHART_VISIBLE).fill(null)),
      mkDataset("Wheel RPM", "#ffaa00", Array(CHART_VISIBLE).fill(null)),
    ],
    0,
    7000,
  ),

  // Dual Y-axis: voltage on left (y), current on right (y1)
  electrical: mkChart(
    "chart-electrical",
    [
      mkDataset("Pack V", "#00e5ff", Array(CHART_VISIBLE).fill(null), "y"),
      mkDataset("Current A", "#ff6b35", Array(CHART_VISIBLE).fill(null), "y1"),
    ],
    0,
    400,
    {
      y1: {
        display: true,
        position: "right",
        grid: { drawOnChartArea: false },
        border: { color: "#222", dash: [2, 4] },
        ticks: {
          color: "#ff6b35",
          font: { family: "'Share Tech Mono'", size: 10 },
          maxTicksLimit: 4,
          callback: (v) => v.toFixed(0) + "A",
        },
      },
    },
  ),

  thermal: mkChart(
    "chart-thermal",
    [
      mkDataset("Avg Temp °C", "#ff4081", Array(CHART_VISIBLE).fill(null)),
      mkDataset("Low Cell V", "#69ff47", Array(CHART_VISIBLE).fill(null)),
    ],
    0,
    80,
  ),

  orientation: mkChart(
    "chart-orientation",
    [
      mkDataset("Yaw °", "#e040fb", Array(CHART_VISIBLE).fill(null)),
      mkDataset("Pitch °", "#00e676", Array(CHART_VISIBLE).fill(null)),
      mkDataset("Roll °", "#40c4ff", Array(CHART_VISIBLE).fill(null)),
    ],
    -180,
    180,
  ),

  speed: mkChart(
    "chart-speed",
    [mkDataset("Speed km/h", "#ffea00", Array(CHART_VISIBLE).fill(null))],
    0,
    140,
  ),

  // Lap times — bar chart (seconds per lap, up to 20 laps)
  laptimes: (function () {
    const ctx = document.getElementById("chart-laptimes");
    if (!ctx) return null;
    return new Chart(ctx, {
      type: "bar",
      data: {
        labels: [],
        datasets: [
          {
            label: "Lap Time (s)",
            data: [],
            backgroundColor: "rgba(64,196,255,0.25)",
            borderColor: "#40c4ff",
            borderWidth: 1.5,
            borderRadius: 3,
          },
        ],
      },
      options: {
        animation: false,
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: { display: false },
          tooltip: {
            enabled: true,
            callbacks: { label: (ctx) => formatLapTime(ctx.parsed.y * 1000) },
          },
        },
        scales: {
          x: {
            grid: { color: "#0e0e0e" },
            ticks: {
              color: "#555",
              font: { family: "'Share Tech Mono'", size: 10 },
            },
          },
          y: {
            grid: { color: "#151515" },
            border: { color: "#222" },
            ticks: {
              color: "#555",
              font: { family: "'Share Tech Mono'", size: 10 },
              callback: (v) => formatLapTime(v * 1000),
            },
          },
        },
      },
    });
  })(),
};

// ── Lap Time Tracking ─────────────────────────────────────
let sfX = null,
  sfY = null; // start/finish local coords
let lapStartTime = null; // Date.now() of lap start
let lapCount = 0,
  bestLapMs = Infinity;
let lastLapMs = null;
let lapCrossedDistance = 0; // distance since last lap start
let lapTimerHandle = null;
const LAP_RADIUS_M = 8; // within 8m of S/F = new lap
const LAP_MIN_DIST = 60; // must travel 60m before SF retriggers

function formatLapTime(ms) {
  if (!isFinite(ms) || ms <= 0) return "—";
  const totalSec = ms / 1000;
  const m = Math.floor(totalSec / 60);
  const s = Math.floor(totalSec % 60);
  const ms2 = Math.floor(ms % 1000);
  return `${m}:${String(s).padStart(2, "0")}.${String(ms2).padStart(3, "0")}`;
}

function setStartFinish() {
  if (positionHistory.length === 0) {
    addLog("No position data yet — drive first.", "err");
    return;
  }
  const last = positionHistory[positionHistory.length - 1];
  sfX = last.x;
  sfY = last.y;
  lapStartTime = Date.now();
  lapCrossedDistance = 0;
  addLog(`S/F set at local (${sfX.toFixed(1)}, ${sfY.toFixed(1)})`, "ok");
  startLapTimer();
}

function startLapTimer() {
  if (lapTimerHandle) clearInterval(lapTimerHandle);
  lapTimerHandle = setInterval(() => {
    if (!lapStartTime) return;
    const el = $("lapCurrent");
    if (!el) return;
    el.textContent = formatLapTime(Date.now() - lapStartTime);
  }, 100);
}

function checkLapTrigger(lx, ly) {
  if (sfX === null || !lapStartTime) return;
  const dist = Math.sqrt((lx - sfX) ** 2 + (ly - sfY) ** 2);
  lapCrossedDistance += 0.1; // rough step — we push every packet
  if (dist < LAP_RADIUS_M && lapCrossedDistance > LAP_MIN_DIST) {
    // Lap complete!
    const lapMs = Date.now() - lapStartTime;
    lapCount++;
    lastLapMs = lapMs;
    if (lapMs < bestLapMs) bestLapMs = lapMs;
    // Update DOM
    $("lapLast").textContent = formatLapTime(lastLapMs);
    $("lapBest").textContent = formatLapTime(bestLapMs);
    $("lapCount").textContent = lapCount;
    // Push to bar chart
    if (charts.laptimes) {
      const ch = charts.laptimes;
      ch.data.labels.push(`L${lapCount}`);
      ch.data.datasets[0].data.push(lapMs / 1000);
      // Colour best lap green
      const cols = ch.data.datasets[0].backgroundColor;
      const bcs = ch.data.datasets[0].borderColor;
      if (!Array.isArray(cols)) {
        ch.data.datasets[0].backgroundColor = Array(lapCount).fill(
          "rgba(64,196,255,0.25)",
        );
        ch.data.datasets[0].borderColor = Array(lapCount).fill("#40c4ff");
      } else {
        ch.data.datasets[0].backgroundColor.push("rgba(64,196,255,0.25)");
        ch.data.datasets[0].borderColor.push("#40c4ff");
      }
      // Mark best lap gold
      const bestIdx = ch.data.datasets[0].data.indexOf(
        Math.min(...ch.data.datasets[0].data),
      );
      ch.data.datasets[0].backgroundColor =
        ch.data.datasets[0].backgroundColor.map((c, i) =>
          i === bestIdx ? "rgba(105,255,71,0.3)" : "rgba(64,196,255,0.25)",
        );
      ch.data.datasets[0].borderColor = ch.data.datasets[0].borderColor.map(
        (c, i) => (i === bestIdx ? "#69ff47" : "#40c4ff"),
      );
      if (ch.data.labels.length > 20) {
        ch.data.labels.shift();
        ch.data.datasets[0].data.shift();
        ch.data.datasets[0].backgroundColor.shift();
        ch.data.datasets[0].borderColor.shift();
      }
      ch.update("none");
    }
    addLog(
      `LAP ${lapCount} — ${formatLapTime(lapMs)}${
        lapMs === bestLapMs ? " 🏆 BEST" : ""
      }`,
      "ok",
    );
    // Reset for next lap
    lapStartTime = Date.now();
    lapCrossedDistance = 0;
  }
}

const GAUGE_CFG = {
  bmsTotal: ["arc-bmsTotal", 0, 378],
  bmsTemp: ["arc-bmsTemp", 0, 80],
  bmsCurrent: ["arc-bmsCurrent", 0, 180],
  bmsLowCell: ["arc-bmsLowCell", 2.5, 4.2],
};
function updateGauge(key, value) {
  const [arcId, min, max] = GAUGE_CFG[key];
  const arc = $(arcId);
  if (!arc) return;
  const ratio = Math.max(0, Math.min(1, (value - min) / (max - min)));
  arc.style.strokeDashoffset = (251.2 * (1 - ratio)).toFixed(2);
}

// ── Angle bar helper ──────────────────────────────────────
function angleBar(deg, range) {
  return Math.max(0, Math.min(100, ((deg + range / 2) / range) * 100));
}

// ── Status pill ───────────────────────────────────────────
function setStatus(state) {
  const dot = $("statusDot"),
    text = $("statusText"),
    pill = $("statusPill"),
    btn = $("connectBtn");
  dot.className = "status-dot";
  pill.className = "status-pill";
  switch (state) {
    case "connected":
      dot.classList.add("on");
      pill.classList.add("connected");
      text.textContent = "CONNECTED (USB)";
      btn.textContent = "DISCONNECT";
      btn.classList.add("danger");
      break;
    case "ws-connected":
      dot.classList.add("on");
      pill.classList.add("connected");
      text.textContent = "CONNECTED (WIFI)";
      btn.textContent = "USB CONNECT";
      btn.classList.remove("danger");
      break;
    case "demo":
      dot.classList.add("demo");
      text.textContent = "DEMO MODE";
      btn.textContent = "CONNECT";
      btn.classList.remove("danger");
      break;
    case "connecting":
      text.textContent = "CONNECTING…";
      btn.textContent = "CANCEL";
      break;
    default:
      text.textContent = "DISCONNECTED";
      btn.textContent = "CONNECT";
      btn.classList.remove("danger");
  }
}

// ─── Seg bar helper ───────────────────────────────────────
function updateSegBar(i, v) {
  const el = $("segBar" + i);
  if (!el) return;
  el.style.width = Math.max(0, Math.min(100, (v / 100) * 100)) + "%";
}

// ═══════════════════════════════════════════════════════════
//  RAF RENDER LOOP — all DOM + canvas writes happen here
// ═══════════════════════════════════════════════════════════
function scheduleRAF() {
  if (!rafScheduled) {
    rafScheduled = true;
    requestAnimationFrame(renderFrame);
  }
}

function pushBuf(key, value, ts) {
  const buf = bufs[key];
  buf.push({ t: ts, v: value });
  if (buf.length > BUF) buf.shift();
}

function getVisible(key) {
  const buf = bufs[key];
  const src = buf.length > CHART_VISIBLE ? buf.slice(-CHART_VISIBLE) : buf;
  const out = Array(CHART_VISIBLE).fill(null);
  for (let i = 0; i < src.length; i++)
    out[CHART_VISIBLE - src.length + i] = src[i].v;
  return out;
}

function renderFrame() {
  rafScheduled = false;

  // 1) Drain pending telemetry into bufs + DOM widgets
  if (pendingChartData) {
    const d = pendingChartData;
    pendingChartData = null;
    const ts = d.timestamp;

    // BMS gauges (DOM)
    $("val-bmsTotal").textContent = d.bmsTotal.toFixed(1);
    $("val-bmsTemp").textContent = d.bmsPackAvgTemp.toFixed(1);
    $("val-bmsCurrent").textContent = d.bmsCurrent.toFixed(1);
    $("val-bmsLowCell").textContent = d.bmsLowestCellV.toFixed(3);
    updateGauge("bmsTotal", d.bmsTotal);
    updateGauge("bmsTemp", d.bmsPackAvgTemp);
    updateGauge("bmsCurrent", d.bmsCurrent);
    updateGauge("bmsLowCell", d.bmsLowestCellV);
    [
      "card-bmsTotal",
      "card-bmsTemp",
      "card-bmsCurrent",
      "card-bmsLowCell",
    ].forEach((id) => {
      const e = $(id);
      if (e) e.classList.add("live");
    });

    // IMU
    $("val-yaw").textContent = d.yaw.toFixed(1) + "°";
    $("val-pitch").textContent = d.pitch.toFixed(1) + "°";
    $("val-roll").textContent = d.roll.toFixed(1) + "°";
    $("bar-yaw").style.width = angleBar(d.yaw, 360) + "%";
    $("bar-pitch").style.width = angleBar(d.pitch, 180) + "%";
    $("bar-roll").style.width = angleBar(d.roll, 360) + "%";
    $("imuCube").style.transform = `rotateX(${(-d.pitch).toFixed(
      1,
    )}deg) rotateY(${d.yaw.toFixed(1)}deg) rotateZ(${(-d.roll).toFixed(1)}deg)`;

    // Speed / motor
    if (d.vehicleSpeed !== undefined) {
      $("val-vehicleSpeed").textContent = d.vehicleSpeed.toFixed(1);
      $("speedBarFill").style.width =
        Math.max(0, Math.min(100, (d.vehicleSpeed / 120) * 100)) + "%";
    }
    if (d.motorSpeed !== undefined)
      $("val-motorRPM").textContent = d.motorSpeed.toFixed(0);
    if (d.wheelSpeed !== undefined)
      $("val-wheelSpeed").textContent = d.wheelSpeed.toFixed(1);

    // VSM
    if (d.vsmState !== undefined) {
      $("val-vsmState").textContent = d.vsmState;
      $("val-vsmName").textContent =
        VSM_NAMES[d.vsmState] || "STATE " + d.vsmState;
    }

    // Segments
    for (let i = 0; i < 5; i++) {
      const v = d["segVolt" + i];
      if (v !== undefined) {
        const e = $("val-seg" + i);
        if (e) e.textContent = v.toFixed(2);
        updateSegBar(i, v);
      }
    }

    // Timestamp / rate
    $("tsValue").textContent = Math.round(d.timestamp).toLocaleString() + " ms";
    $("pktCount").textContent = (++packetCount).toLocaleString();
    $("lastPkt").textContent = new Date().toLocaleTimeString();
    rateCounter++;
    const now = Date.now();
    if (now - rateLastTime >= 1000) {
      $("dataRate").textContent = rateCounter + " Hz";
      rateCounter = 0;
      rateLastTime = now;
    }

    // Position
    if (d.posX !== undefined) updatePosition(d.posX, d.posY, d.posZ, ts);

    // Push to chart bufs
    pushBuf("motorRPM", d.motorSpeed || 0, ts);
    pushBuf("wheelSpeed", d.wheelSpeed || 0, ts);
    pushBuf("packVoltage", d.bmsTotal, ts);
    pushBuf("packCurrent", d.bmsCurrent, ts);
    pushBuf("packTemp", d.bmsPackAvgTemp, ts);
    pushBuf("lowCellV", d.bmsLowestCellV * 20, ts); // scaled x20 to share axis with temp
    pushBuf("yaw", d.yaw, ts);
    pushBuf("pitch", d.pitch, ts);
    pushBuf("roll", d.roll, ts);
    pushBuf("vehicleSpeed", d.vehicleSpeed || 0, ts);

    // Redraw all charts
    if (charts.powertrain) {
      charts.powertrain.data.datasets[0].data = getVisible("motorRPM");
      charts.powertrain.data.datasets[1].data = getVisible("wheelSpeed");
      charts.powertrain.update("none");
    }
    if (charts.electrical) {
      charts.electrical.data.datasets[0].data = getVisible("packVoltage");
      charts.electrical.data.datasets[1].data = getVisible("packCurrent");
      charts.electrical.update("none");
    }
    if (charts.thermal) {
      charts.thermal.data.datasets[0].data = getVisible("packTemp");
      charts.thermal.data.datasets[1].data = getVisible("lowCellV");
      charts.thermal.update("none");
    }
    if (charts.orientation) {
      charts.orientation.data.datasets[0].data = getVisible("yaw");
      charts.orientation.data.datasets[1].data = getVisible("pitch");
      charts.orientation.data.datasets[2].data = getVisible("roll");
      charts.orientation.update("none");
    }
    if (charts.speed) {
      charts.speed.data.datasets[0].data = getVisible("vehicleSpeed");
      charts.speed.update("none");
    }
  }

  // 2) Flush log queue (max 20 entries per frame to stay smooth)
  if (logQueue.length > 0) {
    const body = $("logBody");
    const drain = logQueue.splice(0, 20);
    const frag = document.createDocumentFragment();
    const ts = new Date().toLocaleTimeString("en-GB", { hour12: false });
    for (const { msg, type } of drain) {
      const e = document.createElement("div");
      e.className = "log-entry" + (type ? " " + type : "");
      e.textContent = `[${ts}] ${msg}`;
      frag.appendChild(e);
    }
    body.prepend(frag);
    while (body.children.length > MAX_LOG_LINES)
      body.removeChild(body.lastChild);
    if (logQueue.length > 0) scheduleRAF(); // more to drain next frame
  }
}

// ═══════════════════════════════════════════════════════════
//  3D POSITION  (Three.js)
// ═══════════════════════════════════════════════════════════
let scene3D, camera3D, renderer3D, controls3D;
let trackLine, trackGeometry, trackPositions;
let vehicleSphere, vehicleGlow;
const MAX_TRACK_POINTS = 2000;
let trackPointCount = 0;

function init3DMap() {
  const container = $("threeContainer");
  if (!container || typeof THREE === "undefined") return;
  scene3D = new THREE.Scene();
  scene3D.background = new THREE.Color(0x000000);
  scene3D.fog = new THREE.FogExp2(0x000000, 0.003);
  camera3D = new THREE.PerspectiveCamera(
    60,
    container.clientWidth / container.clientHeight,
    0.1,
    5000,
  );
  camera3D.position.set(30, 40, 30);
  camera3D.lookAt(0, 0, 0);
  renderer3D = new THREE.WebGLRenderer({ antialias: true, alpha: false });
  renderer3D.setSize(container.clientWidth, container.clientHeight);
  renderer3D.setPixelRatio(Math.min(window.devicePixelRatio, 2));
  container.appendChild(renderer3D.domElement);
  if (THREE.OrbitControls) {
    controls3D = new THREE.OrbitControls(camera3D, renderer3D.domElement);
    controls3D.enableDamping = true;
    controls3D.dampingFactor = 0.08;
    controls3D.minDistance = 5;
    controls3D.maxDistance = 500;
    controls3D.maxPolarAngle = Math.PI * 0.85;
  }
  scene3D.add(new THREE.GridHelper(200, 40, 0x330000, 0x1a0505));
  const axLen = 20;
  const mkLine = (p1, p2, c) => {
    const l = new THREE.Line(
      new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(...p1),
        new THREE.Vector3(...p2),
      ]),
      new THREE.LineBasicMaterial({ color: c }),
    );
    scene3D.add(l);
  };
  mkLine([0, 0.05, 0], [axLen, 0.05, 0], 0xff0a0a);
  mkLine([0, 0, 0], [0, axLen, 0], 0x00cc66);
  mkLine([0, 0.05, 0], [0, 0.05, axLen], 0x00cfff);
  const og = new THREE.Mesh(
    new THREE.RingGeometry(0.8, 1.2, 32),
    new THREE.MeshBasicMaterial({ color: 0xff0a0a, side: THREE.DoubleSide }),
  );
  og.rotation.x = -Math.PI / 2;
  og.position.y = 0.02;
  scene3D.add(og);
  trackPositions = new Float32Array(MAX_TRACK_POINTS * 3);
  trackGeometry = new THREE.BufferGeometry();
  trackGeometry.setAttribute(
    "position",
    new THREE.BufferAttribute(trackPositions, 3),
  );
  trackGeometry.setDrawRange(0, 0);
  trackLine = new THREE.Line(
    trackGeometry,
    new THREE.LineBasicMaterial({ color: 0xff3333, linewidth: 2 }),
  );
  scene3D.add(trackLine);
  const sg = new THREE.SphereGeometry(0.6, 16, 16);
  vehicleSphere = new THREE.Mesh(
    sg,
    new THREE.MeshBasicMaterial({ color: 0xff0a0a }),
  );
  vehicleSphere.visible = false;
  scene3D.add(vehicleSphere);
  const gg = new THREE.Mesh(
    new THREE.RingGeometry(0.8, 1.4, 32),
    new THREE.MeshBasicMaterial({
      color: 0xff0a0a,
      transparent: true,
      opacity: 0.3,
      side: THREE.DoubleSide,
    }),
  );
  gg.rotation.x = -Math.PI / 2;
  gg.visible = false;
  scene3D.add(gg);
  vehicleGlow = gg;
  scene3D.add(new THREE.AmbientLight(0xffffff, 0.5));
  window.addEventListener("resize", () => {
    if (!renderer3D) return;
    const w = container.clientWidth,
      h = container.clientHeight;
    camera3D.aspect = w / h;
    camera3D.updateProjectionMatrix();
    renderer3D.setSize(w, h);
  });
  (function animate() {
    requestAnimationFrame(animate);
    if (controls3D) controls3D.update();
    if (vehicleGlow && vehicleGlow.visible) {
      const t = Date.now() * 0.003;
      vehicleGlow.material.opacity = 0.15 + Math.sin(t) * 0.15;
      const s = 1 + Math.sin(t) * 0.2;
      vehicleGlow.scale.set(s, s, s);
    }
    renderer3D.render(scene3D, camera3D);
  })();
}

function updatePosition(ecefX, ecefY, ecefZ, timestamp) {
  if (!originSet) {
    originX = ecefX;
    originY = ecefY;
    originZ = ecefZ;
    originSet = true;
  }
  const localX = ecefX - originX,
    localY = ecefZ - originZ,
    localZ = ecefY - originY;
  $("val-posX").textContent = ecefX.toFixed(2);
  $("val-posY").textContent = ecefY.toFixed(2);
  $("val-posZ").textContent = ecefZ.toFixed(2);
  $("val-altitude").textContent = ecefZ.toFixed(2) + " m";
  if (positionHistory.length > 0) {
    const prev = positionHistory[positionHistory.length - 1];
    const dist = Math.sqrt(
      (localX - prev.x) ** 2 + (localY - prev.y) ** 2 + (localZ - prev.z) ** 2,
    );
    const dt = (timestamp - lastUpdateTime) / 1000;
    if (dt > 0 && dist > 0.01) {
      totalDistance += dist;
      $("val-groundSpeed").textContent = (dist / dt).toFixed(2) + " m/s";
      $("val-totalDistance").textContent = totalDistance.toFixed(1) + " m";
    }
  }
  positionHistory.push({ x: localX, y: localY, z: localZ });
  if (positionHistory.length > MAX_TRACK_POINTS) positionHistory.shift();
  lastPosition = { x: localX, y: localY, z: localZ };
  lastUpdateTime = timestamp;
  if (trackGeometry && trackPointCount < MAX_TRACK_POINTS) {
    const i = trackPointCount * 3;
    trackPositions[i] = localX;
    trackPositions[i + 1] = localY;
    trackPositions[i + 2] = localZ;
    trackPointCount++;
    trackGeometry.attributes.position.needsUpdate = true;
    trackGeometry.setDrawRange(0, trackPointCount);
  }
  if (vehicleSphere) {
    vehicleSphere.position.set(localX, localY, localZ);
    vehicleSphere.visible = true;
    vehicleGlow.position.set(localX, localY + 0.05, localZ);
    vehicleGlow.visible = true;
  }
  const tp = $("val-trackPoints");
  if (tp) tp.textContent = trackPointCount.toString();
  checkLapTrigger(localX, localY);
  updateLeafletMap(ecefX, ecefY, ecefZ);
}

// ═══════════════════════════════════════════════════════════
//  LEAFLET SATELLITE MAP
// ═══════════════════════════════════════════════════════════
const WGS84_A = 6378137.0,
  WGS84_F = 1 / 298.257223563;
const WGS84_B = WGS84_A * (1 - WGS84_F);
const WGS84_E2 = 1 - (WGS84_B * WGS84_B) / (WGS84_A * WGS84_A);
function ecefToLatLon(x, y, z) {
  const lon = Math.atan2(y, x),
    p = Math.sqrt(x * x + y * y);
  let lat = Math.atan2(z, p * (1 - WGS84_E2)),
    N,
    sinLat;
  for (let i = 0; i < 5; i++) {
    sinLat = Math.sin(lat);
    N = WGS84_A / Math.sqrt(1 - WGS84_E2 * sinLat * sinLat);
    lat = Math.atan2(z + WGS84_E2 * N * sinLat, p);
  }
  sinLat = Math.sin(lat);
  N = WGS84_A / Math.sqrt(1 - WGS84_E2 * sinLat * sinLat);
  return {
    lat: lat * (180 / Math.PI),
    lon: lon * (180 / Math.PI),
    alt: p / Math.cos(lat) - N,
  };
}
let leafletMap = null,
  leafletTrack = null,
  leafletMarker = null,
  leafletTrackPoints = [],
  leafletInitialized = false;

function initLeafletMap() {
  const c = $("leafletMap");
  if (!c || typeof L === "undefined") return;
  leafletMap = L.map("leafletMap", {
    center: [12.87, 74.84],
    zoom: 18,
    zoomControl: true,
  });
  L.tileLayer(
    "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
    { attribution: "Tiles: Esri", maxZoom: 20 },
  ).addTo(leafletMap);
  leafletTrack = L.polyline([], {
    color: "#ff0a0a",
    weight: 3,
    opacity: 0.9,
    smoothFactor: 1,
  }).addTo(leafletMap);
  const icon = L.divIcon({
    className: "",
    html: '<div style="width:14px;height:14px;background:#ff0a0a;border-radius:50%;border:2px solid #fff;box-shadow:0 0 12px #ff0a0a,0 0 24px rgba(255,10,10,0.4);"></div>',
    iconSize: [14, 14],
    iconAnchor: [7, 7],
  });
  leafletMarker = L.marker([0, 0], { icon }).addTo(leafletMap);
  leafletMarker.setOpacity(0);
  leafletInitialized = true;
}

function updateLeafletMap(ecefX, ecefY, ecefZ) {
  if (!leafletInitialized || !leafletMap) return;
  if (ecefX === 0 && ecefY === 0 && ecefZ === 0) return;
  const { lat, lon } = ecefToLatLon(ecefX, ecefY, ecefZ);
  if (!isFinite(lat) || !isFinite(lon) || Math.abs(lat) > 90) return;
  const latEl = $("val-lat"),
    lonEl = $("val-lon");
  if (latEl) latEl.textContent = lat.toFixed(6);
  if (lonEl) lonEl.textContent = lon.toFixed(6);
  const latlng = L.latLng(lat, lon);
  if (leafletTrackPoints.length === 0) leafletMap.setView(latlng, 18);
  leafletTrackPoints.push(latlng);
  if (leafletTrackPoints.length > MAX_TRACK_POINTS) leafletTrackPoints.shift();
  leafletTrack.setLatLngs(leafletTrackPoints);
  leafletMarker.setLatLng(latlng);
  leafletMarker.setOpacity(1);
  if (!leafletMap.getBounds().contains(latlng))
    leafletMap.panTo(latlng, { animate: true, duration: 0.5 });
}

// ═══════════════════════════════════════════════════════════
//  applyTelemetry — ONLY stores data, no DOM/canvas writes!
// ═══════════════════════════════════════════════════════════
function applyTelemetry(d) {
  pendingChartData = d; // overwrite with latest — we only want the newest

  // Build CSV row and append to session memory
  const row = [
    d.timestamp !== undefined ? Math.round(d.timestamp) : 0,
    d.bmsTotal !== undefined ? d.bmsTotal.toFixed(2) : "0.00",
    d.bmsPackAvgTemp !== undefined ? d.bmsPackAvgTemp.toFixed(1) : "0.0",
    d.bmsCurrent !== undefined ? d.bmsCurrent.toFixed(1) : "0.0",
    d.bmsLowestCellV !== undefined ? d.bmsLowestCellV.toFixed(3) : "0.000",
    d.motorSpeed !== undefined ? d.motorSpeed.toFixed(1) : "0.0",
    d.segVolt0 !== undefined ? d.segVolt0.toFixed(2) : "0.00",
    d.segVolt1 !== undefined ? d.segVolt1.toFixed(2) : "0.00",
    d.segVolt2 !== undefined ? d.segVolt2.toFixed(2) : "0.00",
    d.segVolt3 !== undefined ? d.segVolt3.toFixed(2) : "0.00",
    d.segVolt4 !== undefined ? d.segVolt4.toFixed(2) : "0.00",
    d.vsmState !== undefined ? d.vsmState : 0,
    d.posX !== undefined ? d.posX.toFixed(4) : "0.0000",
    d.posY !== undefined ? d.posY.toFixed(4) : "0.0000",
    d.posZ !== undefined ? d.posZ.toFixed(4) : "0.0000",
    d.wheelSpeed !== undefined ? d.wheelSpeed.toFixed(1) : "0.0",
    d.vehicleSpeed !== undefined ? d.vehicleSpeed.toFixed(1) : "0.0",
    d.yaw !== undefined ? d.yaw.toFixed(2) : "0.00",
    d.pitch !== undefined ? d.pitch.toFixed(2) : "0.00",
    d.roll !== undefined ? d.roll.toFixed(2) : "0.00",
  ].join(",");
  sessionCsvData.push(row);

  scheduleRAF();
}

// ═══════════════════════════════════════════════════════════
//  PARSER 1 — Binary 79-byte framed packets
// ═══════════════════════════════════════════════════════════
function tryBinaryPackets() {
  let i = 0,
    found = 0;
  while (i <= rxLen - PACKET_SIZE) {
    if (rxBuf[i] === START_BYTE && rxBuf[i + PACKET_SIZE - 1] === END_BYTE) {
      const slice = rxBuf.slice(i, i + PACKET_SIZE);
      const v = new DataView(slice.buffer);
      const d = {
        timestamp: v.getUint32(1, true),
        bmsTotal: v.getFloat32(5, true),
        bmsPackAvgTemp: v.getFloat32(9, true),
        bmsCurrent: v.getFloat32(13, true),
        bmsLowestCellV: v.getFloat32(17, true),
        motorSpeed: v.getFloat32(21, true),
        segVolt0: v.getFloat32(25, true),
        segVolt1: v.getFloat32(29, true),
        segVolt2: v.getFloat32(33, true),
        segVolt3: v.getFloat32(37, true),
        segVolt4: v.getFloat32(41, true),
        vsmState: v.getUint8(45),
        posX: v.getFloat32(46, true),
        posY: v.getFloat32(50, true),
        posZ: v.getFloat32(54, true),
        wheelSpeed: v.getFloat32(58, true),
        vehicleSpeed: v.getFloat32(62, true),
        yaw: v.getFloat32(66, true),
        pitch: v.getFloat32(70, true),
        roll: v.getFloat32(74, true),
      };
      if (
        isFinite(d.bmsTotal) &&
        isFinite(d.yaw) &&
        isFinite(d.motorSpeed) &&
        d.bmsTotal < 600
      ) {
        applyTelemetry(d);
        addLog(
          `[BIN] ✓ T=${d.timestamp}ms V=${d.bmsTotal.toFixed(
            1,
          )}V Motor=${d.motorSpeed.toFixed(0)}RPM VSM=${d.vsmState}`,
          "ok",
        );
        found++;
      }
      i += PACKET_SIZE;
    } else {
      i++;
    }
  }
  if (i > 0) {
    rxBuf.copyWithin(0, i, rxLen);
    rxLen -= i;
  }
  return found > 0;
}

// ═══════════════════════════════════════════════════════════
//  PARSER 2 — CSV
// ═══════════════════════════════════════════════════════════
function tryParseCSV(rawLine) {
  let line = rawLine.trim();
  if (!line) return false;
  const arrow = line.indexOf(" -> ");
  if (arrow !== -1) line = line.substring(arrow + 4).trim();
  for (const p of ["DATA:", "TXT:", "PKT:"]) {
    if (line.startsWith(p)) {
      line = line.substring(p.length).trim();
      break;
    }
  }
  if (
    /^(Qlen|This ESP|Ready|Peer|Teensy|==|ESP-NOW|Warning|ERROR|Queue full|Stats|Parsed|Created|Web server|System|Listening|MAC|Access|Logged|SPIFFS)/.test(
      line,
    )
  )
    return false;
  const parts = line.split(",");
  if (parts.length < 14) return false;
  const vals = parts.map(Number);
  if (vals.slice(0, 5).some(isNaN)) return false;
  if (vals[1] < 0 || vals[1] > 600) return false;
  const t = {
    timestamp: vals[0],
    bmsTotal: vals[1],
    bmsPackAvgTemp: vals[2],
    bmsCurrent: vals[3],
    bmsLowestCellV: vals[4],
    motorSpeed: vals[5],
    segVolt0: vals[6] || 0,
    segVolt1: vals[7] || 0,
    segVolt2: vals[8] || 0,
    segVolt3: vals[9] || 0,
    segVolt4: vals[10] || 0,
    vsmState: vals[11] || 0,
    posX: vals[12] || 0,
    posY: vals[13] || 0,
    posZ: vals[14] || 0,
    wheelSpeed: vals[15] || 0,
    vehicleSpeed: vals[16] || 0,
    yaw: vals[17] || 0,
    pitch: vals[18] || 0,
    roll: vals[19] || 0,
  };
  if (parts.length >= 14 && parts.length < 20) {
    t.yaw = vals[11] || 0;
    t.pitch = vals[12] || 0;
    t.roll = vals[13] || 0;
    t.vsmState = 0;
    t.posX = 0;
    t.posY = 0;
    t.posZ = 0;
    t.wheelSpeed = 0;
    t.vehicleSpeed = 0;
  }
  applyTelemetry(t);
  addLog(
    `[CSV] ✓ T=${vals[0]}ms V=${vals[1].toFixed(1)}V Motor=${vals[5].toFixed(
      0,
    )}RPM`,
    "ok",
  );
  return true;
}

// ═══════════════════════════════════════════════════════════
//  PARSER 3 — JSON
// ═══════════════════════════════════════════════════════════
function tryParseJSON(line) {
  const start = line.indexOf("{");
  if (start === -1) return false;
  const end = line.lastIndexOf("}");
  if (end === -1) return false;
  try {
    const o = JSON.parse(line.substring(start, end + 1));
    const bv = o.bmsTotal || o.bmsV || o.V || o.voltage || 0;
    if (!isFinite(bv)) return false;
    applyTelemetry({
      timestamp: o.timestamp || o.ts || 0,
      bmsTotal: bv,
      bmsPackAvgTemp: o.bmsPackAvgTemp || o.bmsAvgTemp || o.bmsTemp || o.T || 0,
      bmsCurrent: o.bmsCurrent || o.bmsCur || o.I || 0,
      bmsLowestCellV: o.bmsLowestCellV || o.bmsLowCell || 0,
      yaw: o.yaw || 0,
      pitch: o.pitch || 0,
      roll: o.roll || 0,
      motorSpeed: o.motorSpeed || 0,
      wheelSpeed: o.wheelSpeed || 0,
      vehicleSpeed: o.vehicleSpeed || 0,
      segVolt0: o.segVolt0 || 0,
      segVolt1: o.segVolt1 || 0,
      segVolt2: o.segVolt2 || 0,
      segVolt3: o.segVolt3 || 0,
      segVolt4: o.segVolt4 || 0,
      vsmState: o.vsmState || 0,
      posX: o.posX || 0,
      posY: o.posY || 0,
      posZ: o.posZ || 0,
    });
    addLog(`[JSON] T=${o.timestamp || 0}ms V=${bv.toFixed(1)}V`, "ok");
    return true;
  } catch (_) {
    return false;
  }
}

// ── Main chunk handler ─────────────────────────────────────
const textDec = new TextDecoder("utf-8", { fatal: false });
function handleChunk(bytes) {
  for (const b of bytes) {
    if (rxLen < rxBuf.length) rxBuf[rxLen++] = b;
    else {
      rxBuf.copyWithin(0, 256);
      rxLen -= 256;
      rxBuf[rxLen++] = b;
    }
  }
  tryBinaryPackets();
  lineBuf += textDec.decode(bytes, { stream: true });
  const lines = lineBuf.split("\n");
  lineBuf = lines.pop();
  for (const raw of lines) {
    const line = raw.replace(/\r/g, "").trim();
    if (!line) continue;
    addLog(`← ${line.length > 80 ? line.substring(0, 80) + "…" : line}`);
    tryParseJSON(line) || tryParseCSV(line);
  }
}

// ── WebSocket (ESP32 AP) ───────────────────────────────────
function connectWiFi() {
  if (ws) ws.close();
  const host =
    location.hostname === "192.168.4.1"
      ? "192.168.4.1"
      : location.hostname || "192.168.4.1";
  setStatus("connecting");
  addLog(`Attempting WebSocket connection to ws://${host}:81...`, "ok");

  ws = new WebSocket(`ws://${host}:81`);
  ws.binaryType = "arraybuffer";

  ws.onopen = () => {
    isWsConnected = true;
    rxLen = 0;
    lineBuf = "";
    packetCount = 0;
    rateCounter = 0;
    rateLastTime = Date.now();
    sessionCsvData = [];
    setStatus("ws-connected");
    addLog(`WiFi Connected to ${host}! Receiving live binary telemetry.`, "ok");
  };

  ws.onmessage = (e) => {
    if (e.data instanceof ArrayBuffer) {
      handleChunk(new Uint8Array(e.data));
    }
  };

  ws.onclose = () => {
    isWsConnected = false;
    if (!isConnected && !demoMode) setStatus("disconnected");
    addLog("WiFi Disconnected. Reconnecting in 3s...", "err");
    setTimeout(connectWiFi, 3000);
  };

  ws.onerror = (err) => {
    if (!isConnected && !demoMode && !isWsConnected) {
      setStatus("disconnected");
    }
  };
}

// ── Web Serial ─────────────────────────────────────────────
async function connect() {
  try {
    if (!("serial" in navigator)) {
      alert("Web Serial API not supported!\nPlease use Chrome or Edge 89+.");
      return;
    }
    setStatus("connecting");
    port = await navigator.serial.requestPort();
    const baud = parseInt($("baudSelect").value, 10);
    await port.open({ baudRate: baud });
    isConnected = true;
    rxLen = 0;
    lineBuf = "";
    packetCount = 0;
    rateCounter = 0;
    rateLastTime = Date.now();
    sessionCsvData = [];
    setStatus("connected");
    addLog(`Connected @ ${baud} baud — watching for binary/CSV/JSON…`, "ok");
    readLoop();
  } catch (err) {
    setStatus("disconnected");
    if (err.name !== "NotFoundError") addLog("Error: " + err.message, "err");
  }
}
async function disconnect() {
  isConnected = false;
  try {
    if (reader) await reader.cancel();
  } catch (_) {}
  try {
    if (port) await port.close();
  } catch (_) {}
  reader = null;
  port = null;
  setStatus("disconnected");
  addLog(`Disconnected — ${packetCount} packets decoded`, "err");
  $("dataRate").textContent = "- Hz";
}
async function readLoop() {
  reader = port.readable.getReader();
  try {
    while (isConnected) {
      const { value, done } = await reader.read();
      if (done) break;
      if (value && value.length > 0) handleChunk(value);
    }
  } catch (err) {
    if (isConnected) addLog("Read error: " + err.message, "err");
  } finally {
    reader = null;
    if (isConnected) disconnect();
  }
}

// ── Button handlers ────────────────────────────────────────
async function toggleConnect() {
  if (demoMode) stopDemo();
  if (isConnected) disconnect();
  else connect();
}
function toggleDemo() {
  demoMode ? stopDemo() : startDemo();
}
function toggleDiag() {
  addLog("Note: all incoming lines shown in log.", "ok");
}

function startDemo() {
  if (isConnected) disconnect();
  demoMode = true;
  demoT = 0;
  sessionCsvData = [];
  $("demoBtn").classList.add("active");
  setStatus("demo");
  addLog("Demo mode ON — synthetic data @ 50 Hz", "ok");
  demoTimer = setInterval(() => {
    demoT += 0.02;
    const t = demoT;
    const motorRPM = 3000 + Math.sin(t * 0.4) * 1500;
    const wheelRPM = motorRPM / 5.818;
    const vehicleKmh = (wheelRPM * 1.435 * 60) / 1000;
    const trackRadius = 100;
    applyTelemetry({
      timestamp: Math.round(t * 1000),
      bmsTotal: 90 + Math.sin(t * 0.3) * 15,
      bmsPackAvgTemp: 35 + Math.sin(t * 0.2) * 10,
      bmsCurrent: 80 + Math.sin(t * 0.7) * 60,
      bmsLowestCellV: 3.7 + Math.sin(t * 0.15) * 0.2,
      motorSpeed: motorRPM,
      segVolt0: 72 + Math.sin(t * 0.3) * 3,
      segVolt1: 71 + Math.sin(t * 0.32) * 3,
      segVolt2: 73 + Math.sin(t * 0.34) * 3,
      segVolt3: 70 + Math.sin(t * 0.28) * 3,
      segVolt4: 72 + Math.sin(t * 0.36) * 3,
      vsmState: t > 2 ? 6 : t > 1 ? 4 : t > 0.5 ? 2 : 0,
      posX: 1341900 + Math.sin(t * 0.15) * trackRadius,
      posY: 5765700 + Math.cos(t * 0.15) * trackRadius * 0.6,
      posZ: 1410900 + Math.sin(t * 0.05) * 3,
      wheelSpeed: wheelRPM,
      vehicleSpeed: vehicleKmh,
      yaw: ((t * 30) % 360) - 180,
      pitch: Math.sin(t * 0.5) * 20,
      roll: Math.sin(t * 0.8) * 25,
    });
  }, 20); // 50Hz demo
}
function stopDemo() {
  clearInterval(demoTimer);
  demoMode = false;
  $("demoBtn").classList.remove("active");
  setStatus("disconnected");
  addLog("Demo mode OFF");
  $("dataRate").textContent = "- Hz";
}

function downloadCSV() {
  if (sessionCsvData.length === 0) {
    addLog("No data to download yet.", "err");
    return;
  }
  const csvContent = [CSV_HEADER, ...sessionCsvData].join("\n");
  const blob = new Blob([csvContent], { type: "text/csv;charset=utf-8;" });
  const url = URL.createObjectURL(blob);

  const link = document.createElement("a");
  link.setAttribute("href", url);
  const dateStr = new Date().toISOString().replace(/[:.]/g, "-");
  link.setAttribute("download", `telemetry_session_${dateStr}.csv`);
  link.style.visibility = "hidden";

  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);

  addLog(`Downloaded session CSV (${sessionCsvData.length} records)`, "ok");
}

// ── Log (non-blocking — just queues) ──────────────────────
function addLog(msg, type = "") {
  logQueue.push({ msg, type });
  if (logQueue.length > 200) logQueue.splice(0, logQueue.length - 200); // cap
  scheduleRAF();
}
function clearLog() {
  $("logBody").innerHTML = "";
  logQueue = [];
}

// ── Init ──────────────────────────────────────────────────
(function init() {
  addLog("Ready — connect or use DEMO to see data.", "ok");
  if (!("serial" in navigator))
    addLog("Web Serial not available — use Chrome or Edge 89+.", "err");
  init3DMap();

  // Initialize Google Maps instead of Leaflet
  if (typeof initGoogleMap === "function" && typeof google !== "undefined") {
    initGoogleMap();
  } else if (typeof initLeafletMap === "function") {
    initLeafletMap();
  }

  // Auto-connect to WiFi WebSocket if we are loaded from the ESP32 AP
  if (location.hostname === "192.168.4.1" || location.protocol === "http:") {
    connectWiFi();
  }
})();
