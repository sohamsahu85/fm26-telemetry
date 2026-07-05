// ─────────────────────────────────────────────────────────────
//  Formula Manipal — Embedded Dashboard HTML (Auto-Generated)
//  Served from ESP32 AP at 192.168.4.1
//  WebSocket on port 81 receives raw 79-byte binary packets
// ─────────────────────────────────────────────────────────────
#pragma once

static const char DASHBOARD_HTML[] PROGMEM = R"HTMLEOF(
<!DOCTYPE html>
<html lang="en">

<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Formula Manipal — Live Telemetry</title>
  <link rel="preconnect" href="https://fonts.googleapis.com" />
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin />
  <link
    href="https://fonts.googleapis.com/css2?family=Rajdhani:wght@400;500;600;700&family=Share+Tech+Mono&display=swap"
    rel="stylesheet" />
  <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
  <style>
/* ═══════════════════════════════════════════════════════════
   Formula Manipal — Telemetry Dashboard Styles
   Red and black racing theme with neon red accents
═══════════════════════════════════════════════════════════ */

*,
*::before,
*::after {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

:root {
  --bg-base: #050505;
  --bg-card: #0c0c0c;
  --bg-card2: #141414;
  --border: #1e1e1e;
  --border-mid: #2a2a2a;
  --border-glow: rgba(255, 10, 10, 0.25);
  --accent-red: #ff0a0a;
  --accent-red-dim: #cc0808;
  --accent-red-glow: rgba(255, 10, 10, 0.15);
  --accent-orange: #ff6633;
  --accent-yellow: #ffaa00;
  --accent-green: #00cc66;
  --accent-cyan: #00cfff;
  --accent-purple: #bf5af2;
  --text-primary: #f2f2f2;
  --text-secondary: #888888;
  --text-dim: #444444;
  --font-main: 'Rajdhani', sans-serif;
  --font-mono: 'Share Tech Mono', monospace;
  --gauge-track: #181818;
  --radius: 12px;
  --radius-sm: 8px;
  --transition: 0.2s ease;
  --card-shadow: 0 4px 24px rgba(0, 0, 0, 0.6), inset 0 1px 0 rgba(255, 255, 255, 0.03);
  --card-hover-shadow: 0 4px 32px rgba(0, 0, 0, 0.8), 0 0 0 1px rgba(255, 10, 10, 0.15), inset 0 1px 0 rgba(255, 255, 255, 0.04);
}

html,
body {
  height: 100%;
  background: var(--bg-base);
  background-image:
    radial-gradient(circle at 20% 0%, rgba(255, 10, 10, 0.04) 0%, transparent 50%),
    radial-gradient(circle at 80% 100%, rgba(0, 100, 255, 0.02) 0%, transparent 50%),
    url("data:image/svg+xml,%3Csvg width='40' height='40' viewBox='0 0 40 40' xmlns='http://www.w3.org/2000/svg'%3E%3Cpath d='M0 0h1v1H0z' fill='rgba(255,255,255,0.012)'/%3E%3C/svg%3E");
  color: var(--text-primary);
  font-family: var(--font-main);
  font-size: 15px;
  overflow-x: hidden;

}

/* ── Lap Time Card ── */
.lap-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 1rem;
}

@media (max-width: 700px) {
  .lap-grid {
    grid-template-columns: repeat(2, 1fr);
  }
}

.lap-card {
  background: linear-gradient(160deg, #080a14 0%, #05070f 100%);
  border: 1px solid #1a1e2e;
  border-radius: var(--radius);
  padding: 1.1rem 1rem;
  text-align: center;
  box-shadow: var(--card-shadow);
  transition: border-color 0.3s, transform 0.2s;
}

.lap-card:hover {
  border-color: rgba(64, 196, 255, 0.3);
  transform: translateY(-1px);
}

.lap-val {
  font-family: var(--font-mono);
  font-size: 1.5rem;
  font-weight: 700;
  color: var(--accent-cyan);
  text-shadow: 0 0 16px rgba(0, 207, 255, 0.35);
  line-height: 1;
}

.lap-val.best {
  color: #69ff47;
  text-shadow: 0 0 16px rgba(105, 255, 71, 0.35);
}

.lap-lbl {
  font-size: 0.62rem;
  font-weight: 700;
  letter-spacing: 0.16em;
  color: var(--text-dim);
  margin-top: 6px;
  text-transform: uppercase;
}

.btn-set-sf {
  padding: 8px 22px;
  border: 1px solid rgba(64, 196, 255, 0.35);
  border-radius: 6px;
  background: transparent;
  color: var(--accent-cyan);
  font-family: var(--font-main);
  font-size: 0.82rem;
  font-weight: 700;
  letter-spacing: 0.08em;
  cursor: pointer;
  transition: all 0.15s;
}

.btn-set-sf:hover {
  background: rgba(0, 207, 255, 0.08);
  border-color: var(--accent-cyan);
  box-shadow: 0 0 12px rgba(0, 207, 255, 0.15);
}

/* ── Scrollbar ── */
::-webkit-scrollbar {
  width: 6px;
}

::-webkit-scrollbar-track {
  background: var(--bg-base);
}

::-webkit-scrollbar-thumb {
  background: var(--border);
  border-radius: 3px;
}

.mono {
  font-family: var(--font-mono);
}

/* ═══════════════════════ TOP BAR ═══════════════════════ */
.top-bar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 1.8rem;
  height: 68px;
  background: linear-gradient(180deg, #0e0e0e 0%, #080808 100%);
  border-bottom: 1px solid transparent;
  box-shadow:
    0 1px 0 rgba(255, 10, 10, 0.35),
    0 2px 20px rgba(0, 0, 0, 0.8),
    inset 0 1px 0 rgba(255, 255, 255, 0.03);
  position: sticky;
  top: 0;
  z-index: 100;
  backdrop-filter: blur(20px);
}

.top-bar__brand {
  display: flex;
  align-items: center;
  gap: 0.6rem;
}

.brand-car-sticker {
  height: 80px;
  width: auto;
  max-width: 220px;
  object-fit: contain;
  filter: drop-shadow(0 0 8px rgba(255, 10, 10, 0.35));
  transition: transform 0.3s ease, filter 0.3s ease;
}

.brand-car-sticker:hover {
  transform: scale(1.1);
  filter: drop-shadow(0 0 14px rgba(255, 10, 10, 0.6));
}

.brand-dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: var(--accent-red);
  box-shadow: 0 0 12px var(--accent-red);
  animation: pulse-dot 2s ease-in-out infinite;
}

@keyframes pulse-dot {

  0%,
  100% {
    box-shadow: 0 0 6px var(--accent-red);
  }

  50% {
    box-shadow: 0 0 20px var(--accent-red), 0 0 40px rgba(255, 10, 10, 0.4);
  }
}

.brand-name {
  font-size: 1.15rem;
  font-weight: 700;
  letter-spacing: 0.12em;
  color: var(--text-primary);
}

.brand-sub {
  font-size: 0.75rem;
  font-weight: 500;
  letter-spacing: 0.2em;
  color: var(--accent-red);
  padding: 2px 8px;
  border: 1px solid rgba(255, 10, 10, 0.3);
  border-radius: 4px;
}

.top-bar__status {
  display: flex;
  align-items: center;
  gap: 1.2rem;
}

.status-pill {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  padding: 5px 14px;
  border-radius: 20px;
  background: #111111;
  border: 1px solid var(--border);
  font-weight: 600;
  font-size: 0.8rem;
  letter-spacing: 0.1em;
  transition: border-color var(--transition), box-shadow var(--transition);
}

.status-pill.connected {
  border-color: rgba(0, 204, 102, 0.5);
  box-shadow: 0 0 16px rgba(0, 204, 102, 0.15);
}

.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--text-dim);
  transition: background var(--transition), box-shadow var(--transition);
}

.status-dot.on {
  background: var(--accent-green);
  box-shadow: 0 0 10px var(--accent-green);
  animation: blink 1.5s ease-in-out infinite;
}

.status-dot.demo {
  background: var(--accent-yellow);
  box-shadow: 0 0 10px var(--accent-yellow);
  animation: blink 0.8s ease-in-out infinite;
}

@keyframes blink {

  0%,
  100% {
    opacity: 1;
  }

  50% {
    opacity: 0.4;
  }
}

.data-rate {
  font-family: var(--font-mono);
  font-size: 0.85rem;
  color: var(--accent-red);
  min-width: 5ch;
}

.top-bar__controls {
  display: flex;
  align-items: center;
  gap: 0.7rem;
}

.baud-select {
  background: var(--bg-card2);
  border: 1px solid var(--border);
  color: var(--text-secondary);
  padding: 6px 10px;
  border-radius: 6px;
  font-family: var(--font-mono);
  font-size: 0.8rem;
  cursor: pointer;
}

.btn-connect,
.btn-demo {
  padding: 7px 20px;
  border: none;
  border-radius: 6px;
  font-family: var(--font-main);
  font-size: 0.85rem;
  font-weight: 700;
  letter-spacing: 0.08em;
  cursor: pointer;
  transition: all 0.15s ease;
}

.btn-connect {
  background: var(--accent-red);
  color: #fff;
}

.btn-connect:hover {
  filter: brightness(1.15);
  transform: translateY(-1px);
}

.btn-connect.danger {
  background: var(--accent-orange);
}

.btn-demo {
  background: transparent;
  border: 1px solid rgba(255, 170, 0, 0.4);
  color: var(--accent-yellow);
}

.btn-demo:hover {
  border-color: var(--accent-yellow);
  box-shadow: 0 0 12px rgba(255, 170, 0, 0.2);
}

.btn-demo.active {
  background: rgba(255, 170, 0, 0.1);
  border-color: var(--accent-yellow);
}

/* ═══════════════════════ TIMESTAMP BAR ═══════════════════════ */
.ts-bar {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  padding: 7px 1.5rem;
  background: var(--bg-card2);
  border-bottom: 1px solid var(--border);
  font-size: 0.75rem;
}

.ts-label {
  color: var(--text-dim);
  letter-spacing: 0.1em;
  font-weight: 600;
}

.ts-value {
  color: var(--accent-red);
  font-size: 0.8rem;
}

/* ═══════════════════════ MAIN LAYOUT ═══════════════════════ */
.dashboard {
  padding: 1.2rem 1.5rem;
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}

.section-title {
  font-size: 0.72rem;
  font-weight: 700;
  letter-spacing: 0.22em;
  color: var(--text-secondary);
  margin-bottom: 1rem;
  display: flex;
  align-items: center;
  gap: 0.75rem;
  text-transform: uppercase;
  position: relative;
  padding-left: 12px;
}

.section-title::before {
  content: '';
  position: absolute;
  left: 0;
  top: 50%;
  transform: translateY(-50%);
  width: 3px;
  height: 14px;
  background: var(--accent-red);
  border-radius: 2px;
  box-shadow: 0 0 8px var(--accent-red);
}

.section-icon {
  font-size: 1rem;
}

/* ═══════════════════════ VEHICLE DYNAMICS ═══════════════════════ */
.dynamics-grid {
  display: grid;
  grid-template-columns: 1.5fr 1fr 1fr 0.8fr;
  gap: 1rem;
}

@media (max-width: 1000px) {
  .dynamics-grid {
    grid-template-columns: 1fr 1fr;
  }
}

@media (max-width: 600px) {
  .dynamics-grid {
    grid-template-columns: 1fr;
  }
}

.dynamics-card {
  background: linear-gradient(160deg, #0f0f0f 0%, #0a0a0a 100%);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 1.4rem 1.2rem;
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  box-shadow: var(--card-shadow);
  transition: border-color 0.3s, box-shadow 0.3s, transform 0.2s;
}

.dynamics-card:hover {
  border-color: rgba(255, 10, 10, 0.25);
  box-shadow: var(--card-hover-shadow);
  transform: translateY(-1px);
}

.dynamics-card--speed {
  background: linear-gradient(160deg, #110808 0%, #0d0505 50%, #0a0303 100%);
  border-color: rgba(255, 10, 10, 0.3);
  box-shadow:
    0 0 40px rgba(255, 10, 10, 0.08),
    0 4px 24px rgba(0, 0, 0, 0.7),
    inset 0 0 60px rgba(255, 10, 10, 0.04),
    inset 0 1px 0 rgba(255, 255, 255, 0.04);
}

.dynamics-big-value {
  font-size: 4rem;
  font-weight: 700;
  line-height: 1;
  color: var(--accent-red);
  text-shadow:
    0 0 20px rgba(255, 10, 10, 0.6),
    0 0 60px rgba(255, 10, 10, 0.25),
    0 0 100px rgba(255, 10, 10, 0.1);
  letter-spacing: -0.02em;
}

.dynamics-value {
  font-size: 1.8rem;
  font-weight: 700;
  line-height: 1;
  color: var(--text-primary);
}

.dynamics-unit {
  font-size: 0.68rem;
  letter-spacing: 0.12em;
  color: var(--text-secondary);
  margin-top: 4px;
}

.dynamics-label {
  font-size: 0.72rem;
  font-weight: 600;
  letter-spacing: 0.12em;
  color: var(--text-dim);
  margin-top: 0.5rem;
  margin-bottom: 0.5rem;
}

.dynamics-card canvas {
  max-height: 80px;
  width: 100%;
  margin-top: 0.5rem;
}

.speed-bar-track {
  width: 100%;
  height: 6px;
  background: var(--gauge-track);
  border-radius: 3px;
  overflow: hidden;
  margin-top: 0.5rem;
}

.speed-bar-fill {
  height: 100%;
  width: 0%;
  background: linear-gradient(90deg, var(--accent-red), var(--accent-orange));
  border-radius: 3px;
  box-shadow: 0 0 8px rgba(255, 10, 10, 0.5);
  transition: width 0.3s ease;
}

/* VSM State Card */
.dynamics-card--vsm {
  background: linear-gradient(135deg, #0d0d0d 0%, #0d0d1a 100%);
  border-color: rgba(0, 207, 255, 0.2);
  justify-content: center;
}

.vsm-state-label {
  font-size: 0.68rem;
  font-weight: 600;
  letter-spacing: 0.14em;
  color: var(--text-dim);
  margin-bottom: 0.5rem;
}

.vsm-state-value {
  font-size: 2.5rem;
  font-weight: 700;
  color: var(--accent-cyan);
  text-shadow: 0 0 15px rgba(0, 207, 255, 0.3);
  line-height: 1;
}

.vsm-state-name {
  font-size: 0.72rem;
  font-weight: 600;
  letter-spacing: 0.1em;
  color: var(--accent-cyan);
  margin-top: 0.3rem;
  opacity: 0.7;
}

/* ═══════════════════════ GAUGE GRID ═══════════════════════ */
.bms-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 1rem;
}

@media (max-width: 900px) {
  .bms-grid {
    grid-template-columns: repeat(2, 1fr);
  }
}

.gauge-card {
  background: linear-gradient(160deg, #0f0f0f 0%, #0a0a0a 100%);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 1.3rem 1rem 1rem;
  display: flex;
  flex-direction: column;
  align-items: center;
  position: relative;
  box-shadow: var(--card-shadow);
  transition: border-color 0.3s, box-shadow 0.3s, transform 0.2s;
}

.gauge-card.live {
  border-color: rgba(255, 10, 10, 0.35);
  box-shadow:
    0 0 30px rgba(255, 10, 10, 0.1),
    0 4px 24px rgba(0, 0, 0, 0.7),
    inset 0 0 40px rgba(255, 10, 10, 0.04),
    inset 0 1px 0 rgba(255, 255, 255, 0.04);
  transform: translateY(-1px);
}

.gauge-svg {
  width: 130px;
  height: 130px;
}

.gauge-track {
  fill: none;
  stroke: var(--gauge-track);
  stroke-width: 10;
  stroke-linecap: round;
  stroke-dasharray: 251.2;
  stroke-dashoffset: 62.8;
  transform: rotate(-210deg);
  transform-origin: 60px 60px;
}

.gauge-fill {
  fill: none;
  stroke: var(--accent-red);
  stroke-width: 10;
  stroke-linecap: round;
  filter: drop-shadow(0 0 6px var(--accent-red));
  transition: stroke-dashoffset 0.35s cubic-bezier(0.4, 0, 0.2, 1);
}

.gauge-fill--warn {
  stroke: var(--accent-orange);
  filter: drop-shadow(0 0 6px var(--accent-orange));
}

.gauge-fill--current {
  stroke: var(--accent-green);
  filter: drop-shadow(0 0 6px var(--accent-green));
}

.gauge-fill--cell {
  stroke: var(--accent-purple);
  filter: drop-shadow(0 0 6px var(--accent-purple));
}

.gauge-center {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -62%);
  text-align: center;
}

.gauge-value {
  font-size: 1.6rem;
  font-weight: 700;
  line-height: 1;
  font-family: var(--font-mono);
  color: var(--text-primary);
}

.gauge-unit {
  font-size: 0.62rem;
  letter-spacing: 0.12em;
  color: var(--text-secondary);
  margin-top: 2px;
}

.gauge-label {
  font-size: 0.78rem;
  font-weight: 600;
  letter-spacing: 0.1em;
  color: var(--text-secondary);
  margin-top: 0.3rem;
}

.gauge-range {
  font-size: 0.65rem;
  color: var(--text-dim);
  margin-top: 2px;
}

/* ═══════════════════════ SEGMENT VOLTAGES ═══════════════════════ */
.segment-grid {
  display: grid;
  grid-template-columns: repeat(5, 1fr);
  gap: 0.8rem;
}

@media (max-width: 800px) {
  .segment-grid {
    grid-template-columns: repeat(3, 1fr);
  }
}

@media (max-width: 500px) {
  .segment-grid {
    grid-template-columns: repeat(2, 1fr);
  }
}

.seg-card {
  background: linear-gradient(160deg, #0f0f0f 0%, #0a0a0a 100%);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  padding: 1rem 0.8rem;
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  box-shadow: var(--card-shadow);
  transition: border-color 0.3s, transform 0.2s;
}

.seg-card:hover {
  border-color: rgba(255, 170, 0, 0.3);
  transform: translateY(-1px);
}

.seg-number {
  font-size: 0.65rem;
  font-weight: 700;
  letter-spacing: 0.14em;
  color: var(--text-dim);
  background: var(--bg-card2);
  padding: 2px 8px;
  border-radius: 4px;
  margin-bottom: 0.5rem;
}

.seg-value {
  font-size: 1.4rem;
  font-weight: 700;
  color: var(--accent-yellow);
  line-height: 1;
}

.seg-unit {
  font-size: 0.6rem;
  letter-spacing: 0.1em;
  color: var(--text-dim);
  margin-top: 2px;
  margin-bottom: 0.5rem;
}

.seg-bar-track {
  width: 100%;
  height: 4px;
  background: var(--gauge-track);
  border-radius: 2px;
  overflow: hidden;
}

.seg-bar-fill {
  height: 100%;
  width: 0%;
  border-radius: 2px;
  transition: width 0.3s ease;
}

.seg-bar-fill--0 {
  background: #ff6633;
  box-shadow: 0 0 6px rgba(255, 102, 51, 0.4);
}

.seg-bar-fill--1 {
  background: #ffaa00;
  box-shadow: 0 0 6px rgba(255, 170, 0, 0.4);
}

.seg-bar-fill--2 {
  background: #00cc66;
  box-shadow: 0 0 6px rgba(0, 204, 102, 0.4);
}

.seg-bar-fill--3 {
  background: #00cfff;
  box-shadow: 0 0 6px rgba(0, 207, 255, 0.4);
}

.seg-bar-fill--4 {
  background: #bf5af2;
  box-shadow: 0 0 6px rgba(191, 90, 242, 0.4);
}

/* ═══════════════════════ IMU SECTION ═══════════════════════ */
.imu-grid {
  display: grid;
  grid-template-columns: auto 1fr;
  gap: 1rem;
  align-items: center;
}

@media (max-width: 700px) {
  .imu-grid {
    grid-template-columns: 1fr;
  }
}

/* ── 3D Cube ── */
.cube-panel {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.6rem;
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 1.5rem 2rem;
}

.cube-scene {
  width: 130px;
  height: 130px;
  perspective: 400px;
}

.cube {
  width: 100%;
  height: 100%;
  position: relative;
  transform-style: preserve-3d;
  transform: rotateX(-20deg) rotateY(30deg);
  transition: transform 0.15s ease-out;
}

.cube-face {
  position: absolute;
  width: 130px;
  height: 130px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 0.6rem;
  font-weight: 700;
  letter-spacing: 0.1em;
  border: 1px solid rgba(255, 10, 10, 0.25);
  backface-visibility: visible;
}

.cube-face--front {
  background: rgba(255, 10, 10, 0.08);
  transform: translateZ(65px);
  color: var(--accent-red);
}

.cube-face--back {
  background: rgba(255, 10, 10, 0.04);
  transform: rotateY(180deg) translateZ(65px);
  color: rgba(255, 10, 10, 0.5);
}

.cube-face--left {
  background: rgba(255, 10, 10, 0.05);
  transform: rotateY(-90deg) translateZ(65px);
  color: rgba(255, 10, 10, 0.4);
}

.cube-face--right {
  background: rgba(255, 10, 10, 0.05);
  transform: rotateY(90deg) translateZ(65px);
  color: rgba(255, 10, 10, 0.4);
}

.cube-face--top {
  background: rgba(255, 170, 0, 0.07);
  transform: rotateX(90deg) translateZ(65px);
  color: rgba(255, 170, 0, 0.5);
}

.cube-face--bottom {
  background: rgba(255, 170, 0, 0.04);
  transform: rotateX(-90deg) translateZ(65px);
  color: rgba(255, 170, 0, 0.3);
}

.cube-label {
  font-size: 0.7rem;
  font-weight: 600;
  letter-spacing: 0.12em;
  color: var(--text-dim);
}

/* ── Euler bars ── */
.euler-panel {
  background: linear-gradient(160deg, #0f0f0f 0%, #0a0a0a 100%);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 1.6rem 1.8rem;
  display: flex;
  flex-direction: column;
  gap: 1.4rem;
  box-shadow: var(--card-shadow);
}

.euler-row {
  display: grid;
  grid-template-columns: 52px 1fr 80px;
  align-items: center;
  gap: 0.8rem;
}

.euler-name {
  font-size: 0.75rem;
  font-weight: 700;
  letter-spacing: 0.14em;
  color: var(--text-secondary);
}

.euler-bar-wrap {
  height: 8px;
  background: var(--gauge-track);
  border-radius: 4px;
  overflow: hidden;
  position: relative;
}

.euler-bar {
  height: 100%;
  width: 50%;
  background: var(--accent-red);
  border-radius: 4px;
  box-shadow: 0 0 8px rgba(255, 10, 10, 0.5);
  transition: width 0.2s ease;
  transform-origin: left;
}

.euler-bar--pitch {
  background: var(--accent-yellow);
  box-shadow: 0 0 8px rgba(255, 170, 0, 0.5);
}

.euler-bar--roll {
  background: var(--accent-purple);
  box-shadow: 0 0 8px rgba(191, 90, 242, 0.5);
}

.euler-val {
  font-size: 0.9rem;
  color: var(--text-primary);
  text-align: right;
}

/* ═══════════════════════ POSITION MAP ═══════════════════════ */
.position-grid {
  display: grid;
  grid-template-columns: 2fr 1fr;
  gap: 1rem;
}

@media (max-width: 900px) {
  .position-grid {
    grid-template-columns: 1fr;
  }
}

.map-panel {
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  overflow: hidden;
}

.map-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.8rem 1rem;
  background: var(--bg-card2);
  border-bottom: 1px solid var(--border);
}

.map-title {
  font-size: 0.85rem;
  font-weight: 700;
  letter-spacing: 0.12em;
  color: var(--text-secondary);
}

.position-coords {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  font-size: 0.75rem;
}

.coord-label {
  color: var(--text-dim);
  font-weight: 600;
  letter-spacing: 0.08em;
}

.coord-value {
  color: var(--accent-cyan);
  font-family: var(--font-mono);
  min-width: 3ch;
}

.map-container {
  position: relative;
  padding: 1rem;
  background: #000000;
}

#positionMap {
  width: 100%;
  height: 400px;
  border: 1px solid var(--border);
  border-radius: 6px;
  background: radial-gradient(circle at center, #0a0000 0%, #000000 100%);
}

.map-overlay {
  position: absolute;
  top: 1rem;
  right: 1rem;
  pointer-events: none;
}

.map-scale {
  background: rgba(0, 0, 0, 0.8);
  border: 1px solid var(--border);
  border-radius: 4px;
  padding: 4px 8px;
}

.scale-label {
  font-size: 0.65rem;
  color: var(--text-dim);
  font-family: var(--font-mono);
  letter-spacing: 0.08em;
}

.position-stats {
  display: flex;
  flex-direction: column;
  gap: 0.8rem;
}

.stat-card {
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 1rem;
  text-align: center;
  transition: border-color 0.3s;
}

.stat-card:hover {
  border-color: rgba(0, 207, 255, 0.3);
}

.stat-label {
  font-size: 0.7rem;
  font-weight: 600;
  letter-spacing: 0.12em;
  color: var(--text-dim);
  margin-bottom: 0.5rem;
}

.stat-value {
  font-size: 1.1rem;
  font-weight: 700;
  color: var(--accent-cyan);
  font-family: var(--font-mono);
}

/* ═══════════════════════ LOG ═══════════════════════ */
.log-panel {
  margin: 0 1.5rem 1.5rem;
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  overflow: hidden;
}

.log-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.5rem 1rem;
  background: var(--bg-card2);
  border-bottom: 1px solid var(--border);
  font-size: 0.72rem;
  font-weight: 700;
  letter-spacing: 0.14em;
  color: var(--text-dim);
}

.btn-clear {
  background: transparent;
  border: 1px solid var(--border);
  color: var(--text-dim);
  padding: 2px 10px;
  border-radius: 4px;
  font-size: 0.68rem;
  cursor: pointer;
  font-family: var(--font-main);
  letter-spacing: 0.08em;
  transition: all 0.15s;
}

.btn-clear:hover {
  border-color: var(--accent-orange);
  color: var(--accent-orange);
}

.log-body {
  height: 120px;
  overflow-y: auto;
  padding: 0.5rem 1rem;
  font-family: var(--font-mono);
  font-size: 0.72rem;
  color: var(--text-secondary);
  line-height: 1.7;
}

.log-entry {
  border-bottom: 1px solid var(--border);
  padding: 1px 0;
}

.log-entry.err {
  color: var(--accent-orange);
}

.log-entry.ok {
  color: var(--accent-green);
}

/* ═══════════════════════ CHART CARDS (Motor/Wheel) ═══════════════════════ */
.chart-card {
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 1rem;
}

.chart-card__header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 0.6rem;
}

.chart-axis {
  font-size: 0.78rem;
  font-weight: 700;
  letter-spacing: 0.1em;
  padding: 2px 8px;
  border-radius: 4px;
}

.chart-axis--x {
  background: rgba(255, 10, 10, 0.1);
  color: var(--accent-red);
  border: 1px solid rgba(255, 10, 10, 0.2);
}

.chart-axis--y {
  background: rgba(255, 170, 0, 0.1);
  color: var(--accent-yellow);
  border: 1px solid rgba(255, 170, 0, 0.2);
}

.chart-axis--z {
  background: rgba(191, 90, 242, 0.1);
  color: var(--accent-purple);
  border: 1px solid rgba(191, 90, 242, 0.2);
}

.chart-live {
  font-size: 0.82rem;
  color: var(--text-secondary);
}

.chart-card canvas {
  max-height: 130px;
}

/* ═══════════════════════ 3D POSITION MAP ═══════════════════════ */
.position-grid {
  display: grid;
  grid-template-columns: 2fr 1fr;
  gap: 1rem;
}

@media (max-width: 900px) {
  .position-grid {
    grid-template-columns: 1fr;
  }
}

.map-panel {
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  overflow: hidden;
}

.map-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.8rem 1rem;
  background: var(--bg-card2);
  border-bottom: 1px solid var(--border);
}

.map-title {
  font-size: 0.85rem;
  font-weight: 700;
  letter-spacing: 0.12em;
  color: var(--text-secondary);
}

.position-coords {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  font-size: 0.75rem;
}

.coord-label {
  color: var(--text-dim);
  font-weight: 600;
  letter-spacing: 0.08em;
}

.coord-value {
  color: var(--accent-cyan);
  font-family: var(--font-mono);
  min-width: 3ch;
}

#threeContainer {
  width: 100%;
  height: 450px;
  background: #000;
  position: relative;
  cursor: grab;
}

#threeContainer:active {
  cursor: grabbing;
}

#threeContainer canvas {
  display: block;
  width: 100% !important;
  height: 100% !important;
}

.position-stats {
  display: flex;
  flex-direction: column;
  gap: 0.8rem;
}

.stat-card {
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 1rem;
  text-align: center;
  transition: border-color 0.3s;
}

.stat-card:hover {
  border-color: rgba(0, 207, 255, 0.3);
}

.stat-label {
  font-size: 0.7rem;
  font-weight: 600;
  letter-spacing: 0.12em;
  color: var(--text-dim);
  margin-bottom: 0.5rem;
}

.stat-value {
  font-size: 1.1rem;
  font-weight: 700;
  color: var(--accent-cyan);
  font-family: var(--font-mono);
}

/* ═══════════════════════ SATELLITE MAP ═══════════════════════ */
.sat-map-wrapper {
  background: var(--bg-card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  overflow: hidden;
}

.sat-map-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.8rem 1rem;
  background: var(--bg-card2);
  border-bottom: 1px solid var(--border);
}

#leafletMap {
  width: 100%;
  height: 500px;
  background: #0a0a0a;
}

.leaflet-control-zoom a {
  background: #1a1a1a !important;
  color: #ff0a0a !important;
  border-color: #333 !important;
}

.leaflet-control-attribution {
  background: rgba(0, 0, 0, 0.7) !important;
  color: #666 !important;
  font-size: 0.6rem !important;
}

.leaflet-control-attribution a {
  color: #999 !important;
}

/* ═══════════════════════ F1 LIVE GRAPH PANEL ═══════════════════════ */
.graphs-panel {
  display: flex;
  flex-direction: column;
  gap: 0;
  border: 1px solid var(--border);
  border-radius: var(--radius);
  overflow: hidden;
  background: #030303;
  box-shadow: var(--card-shadow);
}

.graph-block {
  border-bottom: 1px solid #0e0e0e;
  position: relative;
}

.graph-block:last-child {
  border-bottom: none;
}

.graph-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 7px 16px 5px;
  background: linear-gradient(90deg, #0a0a0a 0%, #080808 100%);
  border-bottom: 1px solid #0e0e0e;
  position: relative;
  overflow: hidden;
}

.graph-header::before {
  content: '';
  position: absolute;
  left: 0;
  top: 0;
  bottom: 0;
  width: 2px;
  background: var(--accent-red);
  box-shadow: 0 0 6px var(--accent-red);
  opacity: 0.6;
}

.graph-title {
  font-size: 0.65rem;
  font-weight: 700;
  letter-spacing: 0.2em;
  color: var(--text-dim);
  text-transform: uppercase;
  padding-left: 8px;
}

.graph-legend {
  display: flex;
  align-items: center;
  gap: 0.8rem;
}

.legend-pill {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  font-family: var(--font-mono);
  font-size: 0.65rem;
  color: var(--c, #888);
  letter-spacing: 0.04em;
  opacity: 0.85;
}

.legend-pill::before {
  content: '';
  display: inline-block;
  width: 20px;
  height: 2px;
  background: var(--c, #888);
  box-shadow: 0 0 5px var(--c, #888);
  border-radius: 1px;
}

.graph-canvas-wrap {
  height: 180px;
  padding: 4px 0;
  background: #030303;
  position: relative;
}

.graph-canvas-wrap canvas {
  display: block;
  width: 100% !important;
  height: 100% !important;
}
</style>
</head>

<body>

  <!-- ═══════════════════════ HEADER ═══════════════════════ -->
  <header class="top-bar">
    <div class="top-bar__brand">
      <img src="IMG_4700.PNG" alt="Formula Manipal E10" class="brand-car-sticker" />
      <div class="brand-dot"></div>
      <span class="brand-name">FORMULA MANIPAL</span>
      <span class="brand-sub">TELEMETRY</span>
    </div>

    <div class="top-bar__status">
      <div class="status-pill" id="statusPill">
        <span class="status-dot" id="statusDot"></span>
        <span id="statusText">DISCONNECTED</span>
      </div>
      <div class="data-rate" id="dataRate">- Hz</div>
    </div>

    <div class="top-bar__controls">
      <select id="baudSelect" class="baud-select">
        <option value="9600">9600</option>
        <option value="115200" selected>115200</option>
        <option value="230400">230400</option>
        <option value="460800">460800</option>
      </select>
      <button id="connectBtn" class="btn-connect" onclick="toggleConnect()">CONNECT</button>
      <button class="btn-demo" onclick="toggleDemo()" id="demoBtn">DEMO</button>
      <button class="btn-demo" onclick="toggleDiag()" id="diagBtn"
        title="Show raw bytes from serial in the log panel">DIAG</button>
    </div>
  </header>

  <!-- ═══════════════════════ TIMESTAMP BAR ═══════════════════════ -->
  <div class="ts-bar">
    <span class="ts-label">TIMESTAMP</span>
    <span class="ts-value mono" id="tsValue">—</span>
    <span class="ts-label" style="margin-left:2rem">PACKETS RECEIVED</span>
    <span class="ts-value mono" id="pktCount">0</span>
    <span class="ts-label" style="margin-left:2rem">LAST PACKET</span>
    <span class="ts-value mono" id="lastPkt">—</span>
  </div>

  <!-- ═══════════════════════ MAIN GRID ═══════════════════════ -->
  <main class="dashboard">

    <!-- ── VEHICLE DYNAMICS SECTION ── -->
    <section class="section">
      <h2 class="section-title">VEHICLE DYNAMICS</h2>
      <div class="dynamics-grid">
        <div class="dynamics-card dynamics-card--speed">
          <div class="dynamics-big-value mono" id="val-vehicleSpeed">0.0</div>
          <div class="dynamics-unit">km/h</div>
          <div class="dynamics-label">VEHICLE SPEED</div>
          <div class="speed-bar-track">
            <div class="speed-bar-fill" id="speedBarFill"></div>
          </div>
        </div>
        <div class="dynamics-card">
          <div class="dynamics-value mono" id="val-motorRPM">—</div>
          <div class="dynamics-unit">RPM</div>
          <div class="dynamics-label">MOTOR SPEED</div>
        </div>
        <div class="dynamics-card">
          <div class="dynamics-value mono" id="val-wheelSpeed">—</div>
          <div class="dynamics-unit">RPM</div>
          <div class="dynamics-label">WHEEL SPEED</div>
        </div>
        <div class="dynamics-card dynamics-card--vsm">
          <div class="vsm-state-label">VSM STATE</div>
          <div class="vsm-state-value mono" id="val-vsmState">—</div>
          <div class="vsm-state-name" id="val-vsmName">UNKNOWN</div>
        </div>
      </div>
    </section>

    <!-- ── BMS SECTION ── -->
    <section class="section">
      <h2 class="section-title">BATTERY MANAGEMENT SYSTEM</h2>
      <div class="bms-grid">

        <div class="gauge-card" id="card-bmsTotal">
          <svg class="gauge-svg" viewBox="0 0 120 120">
            <circle class="gauge-track" cx="60" cy="60" r="50" />
            <circle class="gauge-fill" id="arc-bmsTotal" cx="60" cy="60" r="50" stroke-dasharray="251.2"
              stroke-dashoffset="251.2" transform="rotate(-210 60 60)" />
          </svg>
          <div class="gauge-center">
            <div class="gauge-value" id="val-bmsTotal">—</div>
            <div class="gauge-unit">VOLTS</div>
          </div>
          <div class="gauge-label">PACK VOLTAGE</div>
          <div class="gauge-range">0 – 378 V</div>
        </div>

        <div class="gauge-card" id="card-bmsTemp">
          <svg class="gauge-svg" viewBox="0 0 120 120">
            <circle class="gauge-track" cx="60" cy="60" r="50" />
            <circle class="gauge-fill gauge-fill--warn" id="arc-bmsTemp" cx="60" cy="60" r="50" stroke-dasharray="251.2"
              stroke-dashoffset="251.2" transform="rotate(-210 60 60)" />
          </svg>
          <div class="gauge-center">
            <div class="gauge-value" id="val-bmsTemp">—</div>
            <div class="gauge-unit">°C</div>
          </div>
          <div class="gauge-label">AVG PACK TEMP</div>
          <div class="gauge-range">0 – 80 °C</div>
        </div>

        <div class="gauge-card" id="card-bmsCurrent">
          <svg class="gauge-svg" viewBox="0 0 120 120">
            <circle class="gauge-track" cx="60" cy="60" r="50" />
            <circle class="gauge-fill gauge-fill--current" id="arc-bmsCurrent" cx="60" cy="60" r="50"
              stroke-dasharray="251.2" stroke-dashoffset="251.2" transform="rotate(-210 60 60)" />
          </svg>
          <div class="gauge-center">
            <div class="gauge-value" id="val-bmsCurrent">—</div>
            <div class="gauge-unit">AMPS</div>
          </div>
          <div class="gauge-label">PACK CURRENT</div>
          <div class="gauge-range">0 – 180 A</div>
        </div>

        <div class="gauge-card" id="card-bmsLowCell">
          <svg class="gauge-svg" viewBox="0 0 120 120">
            <circle class="gauge-track" cx="60" cy="60" r="50" />
            <circle class="gauge-fill gauge-fill--cell" id="arc-bmsLowCell" cx="60" cy="60" r="50"
              stroke-dasharray="251.2" stroke-dashoffset="251.2" transform="rotate(-210 60 60)" />
          </svg>
          <div class="gauge-center">
            <div class="gauge-value" id="val-bmsLowCell">—</div>
            <div class="gauge-unit">VOLTS</div>
          </div>
          <div class="gauge-label">LOWEST CELL V</div>
          <div class="gauge-range">2.5 – 4.2 V</div>
        </div>

      </div>
    </section>

    <!-- ── SEGMENT VOLTAGES SECTION ── -->
    <section class="section">
      <h2 class="section-title">SEGMENT VOLTAGES</h2>
      <div class="segment-grid">
        <div class="seg-card">
          <div class="seg-number">S0</div>
          <div class="seg-value mono" id="val-seg0">—</div>
          <div class="seg-unit">V</div>
          <div class="seg-bar-track">
            <div class="seg-bar-fill seg-bar-fill--0" id="segBar0"></div>
          </div>
        </div>
        <div class="seg-card">
          <div class="seg-number">S1</div>
          <div class="seg-value mono" id="val-seg1">—</div>
          <div class="seg-unit">V</div>
          <div class="seg-bar-track">
            <div class="seg-bar-fill seg-bar-fill--1" id="segBar1"></div>
          </div>
        </div>
        <div class="seg-card">
          <div class="seg-number">S2</div>
          <div class="seg-value mono" id="val-seg2">—</div>
          <div class="seg-unit">V</div>
          <div class="seg-bar-track">
            <div class="seg-bar-fill seg-bar-fill--2" id="segBar2"></div>
          </div>
        </div>
        <div class="seg-card">
          <div class="seg-number">S3</div>
          <div class="seg-value mono" id="val-seg3">—</div>
          <div class="seg-unit">V</div>
          <div class="seg-bar-track">
            <div class="seg-bar-fill seg-bar-fill--3" id="segBar3"></div>
          </div>
        </div>
        <div class="seg-card">
          <div class="seg-number">S4</div>
          <div class="seg-value mono" id="val-seg4">—</div>
          <div class="seg-unit">V</div>
          <div class="seg-bar-track">
            <div class="seg-bar-fill seg-bar-fill--4" id="segBar4"></div>
          </div>
        </div>
      </div>
    </section>

    <!-- ── LAP TIMES SECTION ── -->
    <section class="section">
      <h2 class="section-title">LAP TIMES</h2>
      <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:0.8rem">
        <span style="font-size:0.72rem;color:var(--text-dim);letter-spacing:.1em">Auto-detects lap when car returns
          within 8 m of start/finish</span>
        <button class="btn-set-sf" onclick="setStartFinish()">📍 SET S/F HERE</button>
      </div>
      <div class="lap-grid">
        <div class="lap-card">
          <div class="lap-val" id="lapCurrent">0:00.000</div>
          <div class="lap-lbl">Current Lap</div>
        </div>
        <div class="lap-card">
          <div class="lap-val" id="lapLast">—</div>
          <div class="lap-lbl">Last Lap</div>
        </div>
        <div class="lap-card">
          <div class="lap-val best" id="lapBest">—</div>
          <div class="lap-lbl">Best Lap</div>
        </div>
        <div class="lap-card">
          <div class="lap-val" id="lapCount">0</div>
          <div class="lap-lbl">Lap Count</div>
        </div>
      </div>
    </section>

    <!-- ── F1 LIVE GRAPHS SECTION ── -->
    <section class="section">
      <h2 class="section-title">LIVE DATA GRAPHS</h2>
      <div class="graphs-panel">

        <div class="graph-block">
          <div class="graph-header">
            <span class="graph-title">POWERTRAIN</span>
            <div class="graph-legend">
              <span class="legend-pill" style="--c:#ff2020">Motor RPM</span>
              <span class="legend-pill" style="--c:#ffaa00">Wheel RPM</span>
            </div>
          </div>
          <div class="graph-canvas-wrap"><canvas id="chart-powertrain"></canvas></div>
        </div>

        <div class="graph-block">
          <div class="graph-header">
            <span class="graph-title">BATTERY ELECTRICAL</span>
            <div class="graph-legend">
              <span class="legend-pill" style="--c:#00e5ff">Pack V</span>
              <span class="legend-pill" style="--c:#ff6b35">Current A</span>
            </div>
          </div>
          <div class="graph-canvas-wrap"><canvas id="chart-electrical"></canvas></div>
        </div>

        <div class="graph-block">
          <div class="graph-header">
            <span class="graph-title">THERMALS</span>
            <div class="graph-legend">
              <span class="legend-pill" style="--c:#ff4081">Avg Temp °C</span>
              <span class="legend-pill" style="--c:#69ff47">Low Cell V ×20</span>
            </div>
          </div>
          <div class="graph-canvas-wrap"><canvas id="chart-thermal"></canvas></div>
        </div>

        <div class="graph-block">
          <div class="graph-header">
            <span class="graph-title">ORIENTATION</span>
            <div class="graph-legend">
              <span class="legend-pill" style="--c:#e040fb">Yaw</span>
              <span class="legend-pill" style="--c:#00e676">Pitch</span>
              <span class="legend-pill" style="--c:#40c4ff">Roll</span>
            </div>
          </div>
          <div class="graph-canvas-wrap"><canvas id="chart-orientation"></canvas></div>
        </div>

        <div class="graph-block">
          <div class="graph-header">
            <span class="graph-title">VEHICLE SPEED</span>
            <div class="graph-legend">
              <span class="legend-pill" style="--c:#ffea00">km/h</span>
            </div>
          </div>
          <div class="graph-canvas-wrap"><canvas id="chart-speed"></canvas></div>
        </div>

        <div class="graph-block">
          <div class="graph-header">
            <span class="graph-title">LAP TIMES</span>
            <div class="graph-legend">
              <span class="legend-pill" style="--c:#40c4ff">seconds per lap</span>
            </div>
          </div>
          <div class="graph-canvas-wrap"><canvas id="chart-laptimes"></canvas></div>
        </div>

      </div>
    </section>

    <!-- ── IMU SECTION ── -->
    <section class="section">
      <h2 class="section-title">ORIENTATION (IMU)</h2>
      <div class="imu-grid">

        <!-- 3D Cube -->
        <div class="cube-panel">
          <div class="cube-scene">
            <div class="cube" id="imuCube">
              <div class="cube-face cube-face--front">FRONT</div>
              <div class="cube-face cube-face--back">BACK</div>
              <div class="cube-face cube-face--left">LEFT</div>
              <div class="cube-face cube-face--right">RIGHT</div>
              <div class="cube-face cube-face--top">TOP</div>
              <div class="cube-face cube-face--bottom">BOT</div>
            </div>
          </div>
          <div class="cube-label">3D ORIENTATION</div>
        </div>

        <!-- Yaw / Pitch / Roll numeric + bar -->
        <div class="euler-panel">
          <div class="euler-row">
            <span class="euler-name">YAW</span>
            <div class="euler-bar-wrap">
              <div class="euler-bar" id="bar-yaw"></div>
            </div>
            <span class="euler-val mono" id="val-yaw">—°</span>
          </div>
          <div class="euler-row">
            <span class="euler-name">PITCH</span>
            <div class="euler-bar-wrap">
              <div class="euler-bar euler-bar--pitch" id="bar-pitch"></div>
            </div>
            <span class="euler-val mono" id="val-pitch">—°</span>
          </div>
          <div class="euler-row">
            <span class="euler-name">ROLL</span>
            <div class="euler-bar-wrap">
              <div class="euler-bar euler-bar--roll" id="bar-roll"></div>
            </div>
            <span class="euler-val mono" id="val-roll">—°</span>
          </div>
        </div>
      </div>
    </section>

    <!-- ── 3D POSITION TRACKING SECTION ── -->
    <section class="section">
      <h2 class="section-title">3D POSITION TRACKING</h2>
      <div class="position-grid">
        <div class="map-panel">
          <div class="map-header">
            <span class="map-title">LIVE 3D TRACK (VN-200 ECEF)</span>
            <div class="position-coords">
              <span class="coord-label">X:</span>
              <span class="coord-value mono" id="val-posX">—</span>
              <span class="coord-label">Y:</span>
              <span class="coord-value mono" id="val-posY">—</span>
              <span class="coord-label">Z:</span>
              <span class="coord-value mono" id="val-posZ">—</span>
            </div>
          </div>
          <div class="map-container" id="threeContainer"></div>
        </div>
        <div class="position-stats">
          <div class="stat-card">
            <div class="stat-label">ALTITUDE (Z)</div>
            <div class="stat-value mono" id="val-altitude">— m</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">GROUND SPEED</div>
            <div class="stat-value mono" id="val-groundSpeed">— m/s</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">TOTAL DISTANCE</div>
            <div class="stat-value mono" id="val-totalDistance">— m</div>
          </div>
          <div class="stat-card">
            <div class="stat-label">TRACK POINTS</div>
            <div class="stat-value mono" id="val-trackPoints">0</div>
          </div>
        </div>
      </div>
    </section>

    <!-- ── SATELLITE MAP SECTION ── -->
    <section class="section">
      <h2 class="section-title">SATELLITE MAP</h2>
      <div class="sat-map-wrapper">
        <div class="sat-map-header">
          <span class="map-title">LIVE GPS TRACK</span>
          <div class="position-coords">
            <span class="coord-label">LAT:</span>
            <span class="coord-value mono" id="val-lat">—</span>
            <span class="coord-label">LON:</span>
            <span class="coord-value mono" id="val-lon">—</span>
          </div>
        </div>
        <div id="leafletMap"></div>
      </div>
    </section>
    <div class="log-body" id="logBody"></div>
  </div>

  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/three@0.148.0/build/three.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/three@0.148.0/examples/js/controls/OrbitControls.js"></script>

  <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
  <script>
// ═══════════════════════════════════════════════════════════
//  Formula Manipal — Telemetry Dashboard  v3
//  Binary 79-byte packets + text/CSV/JSON parser
//  RAF render loop — decoupled from receive path for max Hz
// ═══════════════════════════════════════════════════════════
'use strict';

const PACKET_SIZE = 79;
const START_BYTE = 0xAA;
const END_BYTE = 0x55;
const MAX_LOG_LINES = 80;
const CHART_POINTS = 300;   // points kept in memory
const CHART_VISIBLE = 200;   // points rendered (last 200)

// ── State ─────────────────────────────────────────────────
let port = null, reader = null;
let isConnected = false, demoMode = false;
let ws = null, isWsConnected = false;
let demoTimer = null, demoT = 0;
let packetCount = 0, rateCounter = 0, rateLastTime = Date.now();

// Position
let positionHistory = [], lastPosition = { x: 0, y: 0, z: 0 };
let totalDistance = 0, lastUpdateTime = 0;
let originSet = false, originX = 0, originY = 0, originZ = 0;

// Binary buffer
let rxBuf = new Uint8Array(4096), rxLen = 0;
let lineBuf = '';

// ── RAF-batched log ───────────────────────────────────────
let logQueue = [];   // { msg, type }
let rafScheduled = false;

// ── Chart data buffers ────────────────────────────────────
// Each buffer is a ring of { t, v } objects (t = ms timestamp)
const BUF = CHART_POINTS;
const bufs = {
  motorRPM: [], wheelSpeed: [],
  packVoltage: [], packCurrent: [],
  packTemp: [], lowCellV: [],
  yaw: [], pitch: [], roll: [],
  vehicleSpeed: []
};

// Pending values to push to charts on next RAF frame
let pendingChartData = null;

// ── VSM State Names ───────────────────────────────────────
const VSM_NAMES = {
  0: 'VSM START', 1: 'PRE-CHARGE INIT', 2: 'PRE-CHARGE ACTIVE',
  3: 'PRE-CHARGE COMPLETE', 4: 'VSM WAIT', 5: 'VSM READY',
  6: 'MOTOR RUNNING', 7: 'BLINK', 14: 'SHUTDOWN', 15: 'RECYCLE'
};

// ── DOM helper ────────────────────────────────────────────
function $(id) { return document.getElementById(id); }

// ═══════════════════════════════════════════════════════════
//  CHART SETUP  — F1 pitwall style
// ═══════════════════════════════════════════════════════════
Chart.defaults.color = '#666';
Chart.defaults.borderColor = '#151515';

// Plugin: draw current value label at right edge of each dataset
const currentValuePlugin = {
  id: 'currentValue',
  afterDatasetsDraw(chart) {
    const ctx = chart.ctx;
    chart.data.datasets.forEach((ds, i) => {
      const meta = chart.getDatasetMeta(i);
      if (!meta.visible) return;
      // Find last non-null point
      let lastPt = null;
      for (let j = ds.data.length - 1; j >= 0; j--) {
        if (ds.data[j] !== null && isFinite(ds.data[j])) {
          lastPt = meta.data[j]; break;
        }
      }
      if (!lastPt) return;
      const val = ds.data.filter(v => v !== null && isFinite(v));
      if (!val.length) return;
      const latest = val[val.length - 1];
      const label = Math.abs(latest) >= 100 ? latest.toFixed(0) :
        Math.abs(latest) >= 10 ? latest.toFixed(1) : latest.toFixed(2);
      ctx.save();
      ctx.font = `bold 9px 'Share Tech Mono', monospace`;
      ctx.fillStyle = ds.borderColor;
      ctx.textAlign = 'right';
      ctx.shadowColor = ds.borderColor;
      ctx.shadowBlur = 4;
      ctx.fillText(label, chart.chartArea.right - 4, lastPt.y - 4);
      ctx.restore();
    });
  }
};
Chart.register(currentValuePlugin);

function mkDataset(label, color, data, yAxisID = 'y') {
  return {
    label, data, yAxisID,
    borderColor: color,
    borderWidth: 2,
    pointRadius: 0,
    tension: 0.2,
    fill: false,
    backgroundColor: 'transparent'
  };
}

function mkChart(id, datasets, yMin, yMax, extraScales = {}) {
  const ctx = document.getElementById(id);
  if (!ctx) return null;
  return new Chart(ctx, {
    type: 'line',
    data: { labels: Array(CHART_VISIBLE).fill(''), datasets },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: false,
      interaction: { mode: 'index', intersect: false },
      plugins: {
        legend: { display: false },
        tooltip: { enabled: false }
      },
      scales: {
        x: { display: false },
        y: {
          display: true,
          min: yMin,
          max: yMax,
          position: 'left',
          grid: { color: '#151515', drawBorder: false },
          border: { color: '#222', dash: [2, 4] },
          ticks: {
            color: '#555',
            font: { family: "'Share Tech Mono'", size: 10 },
            maxTicksLimit: 4,
            callback: v => Math.abs(v) >= 100 ? v.toFixed(0) :
              Math.abs(v) >= 10 ? v.toFixed(1) : v.toFixed(2)
          }
        },
        ...extraScales
      }
    }
  });
}

const charts = {
  powertrain: mkChart('chart-powertrain', [
    mkDataset('Motor RPM', '#ff2020', Array(CHART_VISIBLE).fill(null)),
    mkDataset('Wheel RPM', '#ffaa00', Array(CHART_VISIBLE).fill(null))
  ], 0, 7000),

  // Dual Y-axis: voltage on left (y), current on right (y1)
  electrical: mkChart('chart-electrical', [
    mkDataset('Pack V', '#00e5ff', Array(CHART_VISIBLE).fill(null), 'y'),
    mkDataset('Current A', '#ff6b35', Array(CHART_VISIBLE).fill(null), 'y1')
  ], 0, 400, {
    y1: {
      display: true,
      position: 'right',
      grid: { drawOnChartArea: false },
      border: { color: '#222', dash: [2, 4] },
      ticks: {
        color: '#ff6b35',
        font: { family: "'Share Tech Mono'", size: 10 },
        maxTicksLimit: 4,
        callback: v => v.toFixed(0) + 'A'
      }
    }
  }),

  thermal: mkChart('chart-thermal', [
    mkDataset('Avg Temp °C', '#ff4081', Array(CHART_VISIBLE).fill(null)),
    mkDataset('Low Cell V', '#69ff47', Array(CHART_VISIBLE).fill(null))
  ], 0, 80),

  orientation: mkChart('chart-orientation', [
    mkDataset('Yaw °', '#e040fb', Array(CHART_VISIBLE).fill(null)),
    mkDataset('Pitch °', '#00e676', Array(CHART_VISIBLE).fill(null)),
    mkDataset('Roll °', '#40c4ff', Array(CHART_VISIBLE).fill(null))
  ], -180, 180),

  speed: mkChart('chart-speed', [
    mkDataset('Speed km/h', '#ffea00', Array(CHART_VISIBLE).fill(null))
  ], 0, 140),

  // Lap times — bar chart (seconds per lap, up to 20 laps)
  laptimes: (function () {
    const ctx = document.getElementById('chart-laptimes');
    if (!ctx) return null;
    return new Chart(ctx, {
      type: 'bar',
      data: {
        labels: [],
        datasets: [{
          label: 'Lap Time (s)',
          data: [],
          backgroundColor: 'rgba(64,196,255,0.25)',
          borderColor: '#40c4ff',
          borderWidth: 1.5,
          borderRadius: 3,
        }]
      },
      options: {
        animation: false, responsive: true, maintainAspectRatio: false,
        plugins: {
          legend: { display: false },
          tooltip: {
            enabled: true,
            callbacks: { label: ctx => formatLapTime(ctx.parsed.y * 1000) }
          }
        },
        scales: {
          x: { grid: { color: '#0e0e0e' }, ticks: { color: '#555', font: { family: "'Share Tech Mono'", size: 10 } } },
          y: {
            grid: { color: '#151515' }, border: { color: '#222' },
            ticks: {
              color: '#555', font: { family: "'Share Tech Mono'", size: 10 },
              callback: v => formatLapTime(v * 1000)
            }
          }
        }
      }
    });
  })()
};

// ── Lap Time Tracking ─────────────────────────────────────
let sfX = null, sfY = null;            // start/finish local coords
let lapStartTime = null;               // Date.now() of lap start
let lapCount = 0, bestLapMs = Infinity;
let lastLapMs = null;
let lapCrossedDistance = 0;            // distance since last lap start
let lapTimerHandle = null;
const LAP_RADIUS_M = 8;               // within 8m of S/F = new lap
const LAP_MIN_DIST = 60;              // must travel 60m before SF retriggers

function formatLapTime(ms) {
  if (!isFinite(ms) || ms <= 0) return '—';
  const totalSec = ms / 1000;
  const m = Math.floor(totalSec / 60);
  const s = Math.floor(totalSec % 60);
  const ms2 = Math.floor(ms % 1000);
  return `${m}:${String(s).padStart(2, '0')}.${String(ms2).padStart(3, '0')}`;
}

function setStartFinish() {
  if (positionHistory.length === 0) { addLog('No position data yet — drive first.', 'err'); return; }
  const last = positionHistory[positionHistory.length - 1];
  sfX = last.x; sfY = last.y;
  lapStartTime = Date.now(); lapCrossedDistance = 0;
  addLog(`S/F set at local (${sfX.toFixed(1)}, ${sfY.toFixed(1)})`, 'ok');
  startLapTimer();
}

function startLapTimer() {
  if (lapTimerHandle) clearInterval(lapTimerHandle);
  lapTimerHandle = setInterval(() => {
    if (!lapStartTime) return;
    const el = $('lapCurrent'); if (!el) return;
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
    $('lapLast').textContent = formatLapTime(lastLapMs);
    $('lapBest').textContent = formatLapTime(bestLapMs);
    $('lapCount').textContent = lapCount;
    // Push to bar chart
    if (charts.laptimes) {
      const ch = charts.laptimes;
      ch.data.labels.push(`L${lapCount}`);
      ch.data.datasets[0].data.push((lapMs / 1000));
      // Colour best lap green
      const cols = ch.data.datasets[0].backgroundColor;
      const bcs = ch.data.datasets[0].borderColor;
      if (!Array.isArray(cols)) {
        ch.data.datasets[0].backgroundColor = Array(lapCount).fill('rgba(64,196,255,0.25)');
        ch.data.datasets[0].borderColor = Array(lapCount).fill('#40c4ff');
      } else {
        ch.data.datasets[0].backgroundColor.push('rgba(64,196,255,0.25)');
        ch.data.datasets[0].borderColor.push('#40c4ff');
      }
      // Mark best lap gold
      const bestIdx = ch.data.datasets[0].data.indexOf(Math.min(...ch.data.datasets[0].data));
      ch.data.datasets[0].backgroundColor = ch.data.datasets[0].backgroundColor.map(
        (c, i) => i === bestIdx ? 'rgba(105,255,71,0.3)' : 'rgba(64,196,255,0.25)');
      ch.data.datasets[0].borderColor = ch.data.datasets[0].borderColor.map(
        (c, i) => i === bestIdx ? '#69ff47' : '#40c4ff');
      if (ch.data.labels.length > 20) {
        ch.data.labels.shift(); ch.data.datasets[0].data.shift();
        ch.data.datasets[0].backgroundColor.shift(); ch.data.datasets[0].borderColor.shift();
      }
      ch.update('none');
    }
    addLog(`LAP ${lapCount} — ${formatLapTime(lapMs)}${lapMs === bestLapMs ? ' 🏆 BEST' : ''}`, 'ok');
    // Reset for next lap
    lapStartTime = Date.now(); lapCrossedDistance = 0;
  }
}


const GAUGE_CFG = {
  bmsTotal: ['arc-bmsTotal', 0, 378],
  bmsTemp: ['arc-bmsTemp', 0, 80],
  bmsCurrent: ['arc-bmsCurrent', 0, 180],
  bmsLowCell: ['arc-bmsLowCell', 2.5, 4.2],
};
function updateGauge(key, value) {
  const [arcId, min, max] = GAUGE_CFG[key];
  const arc = $(arcId); if (!arc) return;
  const ratio = Math.max(0, Math.min(1, (value - min) / (max - min)));
  arc.style.strokeDashoffset = (251.2 * (1 - ratio)).toFixed(2);
}

// ── Angle bar helper ──────────────────────────────────────
function angleBar(deg, range) {
  return Math.max(0, Math.min(100, ((deg + range / 2) / range) * 100));
}

// ── Status pill ───────────────────────────────────────────
function setStatus(state) {
  const dot = $('statusDot'), text = $('statusText'),
    pill = $('statusPill'), btn = $('connectBtn');
  dot.className = 'status-dot'; pill.className = 'status-pill';
  switch (state) {
    case 'connected':
      dot.classList.add('on'); pill.classList.add('connected');
      text.textContent = 'CONNECTED (USB)'; btn.textContent = 'DISCONNECT';
      btn.classList.add('danger'); break;
    case 'ws-connected':
      dot.classList.add('on'); pill.classList.add('connected');
      text.textContent = 'CONNECTED (WIFI)'; btn.textContent = 'USB CONNECT';
      btn.classList.remove('danger'); break;
    case 'demo':
      dot.classList.add('demo'); text.textContent = 'DEMO MODE';
      btn.textContent = 'CONNECT'; btn.classList.remove('danger'); break;
    case 'connecting':
      text.textContent = 'CONNECTING…'; btn.textContent = 'CANCEL'; break;
    default:
      text.textContent = 'DISCONNECTED'; btn.textContent = 'CONNECT';
      btn.classList.remove('danger');
  }
}

// ─── Seg bar helper ───────────────────────────────────────
function updateSegBar(i, v) {
  const el = $('segBar' + i); if (!el) return;
  el.style.width = Math.max(0, Math.min(100, (v / 100) * 100)) + '%';
}

// ═══════════════════════════════════════════════════════════
//  RAF RENDER LOOP — all DOM + canvas writes happen here
// ═══════════════════════════════════════════════════════════
function scheduleRAF() {
  if (!rafScheduled) { rafScheduled = true; requestAnimationFrame(renderFrame); }
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
  for (let i = 0; i < src.length; i++) out[CHART_VISIBLE - src.length + i] = src[i].v;
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
    $('val-bmsTotal').textContent = d.bmsTotal.toFixed(1);
    $('val-bmsTemp').textContent = d.bmsPackAvgTemp.toFixed(1);
    $('val-bmsCurrent').textContent = d.bmsCurrent.toFixed(1);
    $('val-bmsLowCell').textContent = d.bmsLowestCellV.toFixed(3);
    updateGauge('bmsTotal', d.bmsTotal);
    updateGauge('bmsTemp', d.bmsPackAvgTemp);
    updateGauge('bmsCurrent', d.bmsCurrent);
    updateGauge('bmsLowCell', d.bmsLowestCellV);
    ['card-bmsTotal', 'card-bmsTemp', 'card-bmsCurrent', 'card-bmsLowCell']
      .forEach(id => { const e = $(id); if (e) e.classList.add('live'); });

    // IMU
    $('val-yaw').textContent = d.yaw.toFixed(1) + '°';
    $('val-pitch').textContent = d.pitch.toFixed(1) + '°';
    $('val-roll').textContent = d.roll.toFixed(1) + '°';
    $('bar-yaw').style.width = angleBar(d.yaw, 360) + '%';
    $('bar-pitch').style.width = angleBar(d.pitch, 180) + '%';
    $('bar-roll').style.width = angleBar(d.roll, 360) + '%';
    $('imuCube').style.transform =
      `rotateX(${(-d.pitch).toFixed(1)}deg) rotateY(${d.yaw.toFixed(1)}deg) rotateZ(${(-d.roll).toFixed(1)}deg)`;

    // Speed / motor
    if (d.vehicleSpeed !== undefined) {
      $('val-vehicleSpeed').textContent = d.vehicleSpeed.toFixed(1);
      $('speedBarFill').style.width = Math.max(0, Math.min(100, (d.vehicleSpeed / 120) * 100)) + '%';
    }
    if (d.motorSpeed !== undefined) $('val-motorRPM').textContent = d.motorSpeed.toFixed(0);
    if (d.wheelSpeed !== undefined) $('val-wheelSpeed').textContent = d.wheelSpeed.toFixed(1);

    // VSM
    if (d.vsmState !== undefined) {
      $('val-vsmState').textContent = d.vsmState;
      $('val-vsmName').textContent = VSM_NAMES[d.vsmState] || 'STATE ' + d.vsmState;
    }

    // Segments
    for (let i = 0; i < 5; i++) {
      const v = d['segVolt' + i];
      if (v !== undefined) { const e = $('val-seg' + i); if (e) e.textContent = v.toFixed(2); updateSegBar(i, v); }
    }

    // Timestamp / rate
    $('tsValue').textContent = Math.round(d.timestamp).toLocaleString() + ' ms';
    $('pktCount').textContent = (++packetCount).toLocaleString();
    $('lastPkt').textContent = new Date().toLocaleTimeString();
    rateCounter++;
    const now = Date.now();
    if (now - rateLastTime >= 1000) {
      $('dataRate').textContent = rateCounter + ' Hz';
      rateCounter = 0; rateLastTime = now;
    }

    // Position
    if (d.posX !== undefined) updatePosition(d.posX, d.posY, d.posZ, ts);

    // Push to chart bufs
    pushBuf('motorRPM', d.motorSpeed || 0, ts);
    pushBuf('wheelSpeed', d.wheelSpeed || 0, ts);
    pushBuf('packVoltage', d.bmsTotal, ts);
    pushBuf('packCurrent', d.bmsCurrent, ts);
    pushBuf('packTemp', d.bmsPackAvgTemp, ts);
    pushBuf('lowCellV', d.bmsLowestCellV * 20, ts); // scaled x20 to share axis with temp
    pushBuf('yaw', d.yaw, ts);
    pushBuf('pitch', d.pitch, ts);
    pushBuf('roll', d.roll, ts);
    pushBuf('vehicleSpeed', d.vehicleSpeed || 0, ts);

    // Redraw all charts
    if (charts.powertrain) {
      charts.powertrain.data.datasets[0].data = getVisible('motorRPM');
      charts.powertrain.data.datasets[1].data = getVisible('wheelSpeed');
      charts.powertrain.update('none');
    }
    if (charts.electrical) {
      charts.electrical.data.datasets[0].data = getVisible('packVoltage');
      charts.electrical.data.datasets[1].data = getVisible('packCurrent');
      charts.electrical.update('none');
    }
    if (charts.thermal) {
      charts.thermal.data.datasets[0].data = getVisible('packTemp');
      charts.thermal.data.datasets[1].data = getVisible('lowCellV');
      charts.thermal.update('none');
    }
    if (charts.orientation) {
      charts.orientation.data.datasets[0].data = getVisible('yaw');
      charts.orientation.data.datasets[1].data = getVisible('pitch');
      charts.orientation.data.datasets[2].data = getVisible('roll');
      charts.orientation.update('none');
    }
    if (charts.speed) {
      charts.speed.data.datasets[0].data = getVisible('vehicleSpeed');
      charts.speed.update('none');
    }
  }

  // 2) Flush log queue (max 20 entries per frame to stay smooth)
  if (logQueue.length > 0) {
    const body = $('logBody');
    const drain = logQueue.splice(0, 20);
    const frag = document.createDocumentFragment();
    const ts = new Date().toLocaleTimeString('en-GB', { hour12: false });
    for (const { msg, type } of drain) {
      const e = document.createElement('div');
      e.className = 'log-entry' + (type ? ' ' + type : '');
      e.textContent = `[${ts}] ${msg}`;
      frag.appendChild(e);
    }
    body.prepend(frag);
    while (body.children.length > MAX_LOG_LINES) body.removeChild(body.lastChild);
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
  const container = $('threeContainer');
  if (!container || typeof THREE === 'undefined') return;
  scene3D = new THREE.Scene();
  scene3D.background = new THREE.Color(0x000000);
  scene3D.fog = new THREE.FogExp2(0x000000, 0.003);
  camera3D = new THREE.PerspectiveCamera(60, container.clientWidth / container.clientHeight, 0.1, 5000);
  camera3D.position.set(30, 40, 30); camera3D.lookAt(0, 0, 0);
  renderer3D = new THREE.WebGLRenderer({ antialias: true, alpha: false });
  renderer3D.setSize(container.clientWidth, container.clientHeight);
  renderer3D.setPixelRatio(Math.min(window.devicePixelRatio, 2));
  container.appendChild(renderer3D.domElement);
  if (THREE.OrbitControls) {
    controls3D = new THREE.OrbitControls(camera3D, renderer3D.domElement);
    controls3D.enableDamping = true; controls3D.dampingFactor = 0.08;
    controls3D.minDistance = 5; controls3D.maxDistance = 500;
    controls3D.maxPolarAngle = Math.PI * 0.85;
  }
  scene3D.add(new THREE.GridHelper(200, 40, 0x330000, 0x1a0505));
  const axLen = 20;
  const mkLine = (p1, p2, c) => { const l = new THREE.Line(new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(...p1), new THREE.Vector3(...p2)]), new THREE.LineBasicMaterial({ color: c })); scene3D.add(l); };
  mkLine([0, 0.05, 0], [axLen, 0.05, 0], 0xff0a0a);
  mkLine([0, 0, 0], [0, axLen, 0], 0x00cc66);
  mkLine([0, 0.05, 0], [0, 0.05, axLen], 0x00cfff);
  const og = new THREE.Mesh(new THREE.RingGeometry(0.8, 1.2, 32), new THREE.MeshBasicMaterial({ color: 0xff0a0a, side: THREE.DoubleSide }));
  og.rotation.x = -Math.PI / 2; og.position.y = 0.02; scene3D.add(og);
  trackPositions = new Float32Array(MAX_TRACK_POINTS * 3);
  trackGeometry = new THREE.BufferGeometry();
  trackGeometry.setAttribute('position', new THREE.BufferAttribute(trackPositions, 3));
  trackGeometry.setDrawRange(0, 0);
  trackLine = new THREE.Line(trackGeometry, new THREE.LineBasicMaterial({ color: 0xff3333, linewidth: 2 }));
  scene3D.add(trackLine);
  const sg = new THREE.SphereGeometry(0.6, 16, 16);
  vehicleSphere = new THREE.Mesh(sg, new THREE.MeshBasicMaterial({ color: 0xff0a0a }));
  vehicleSphere.visible = false; scene3D.add(vehicleSphere);
  const gg = new THREE.Mesh(new THREE.RingGeometry(0.8, 1.4, 32), new THREE.MeshBasicMaterial({ color: 0xff0a0a, transparent: true, opacity: 0.3, side: THREE.DoubleSide }));
  gg.rotation.x = -Math.PI / 2; gg.visible = false; scene3D.add(gg); vehicleGlow = gg;
  scene3D.add(new THREE.AmbientLight(0xffffff, 0.5));
  window.addEventListener('resize', () => {
    if (!renderer3D) return;
    const w = container.clientWidth, h = container.clientHeight;
    camera3D.aspect = w / h; camera3D.updateProjectionMatrix(); renderer3D.setSize(w, h);
  });
  (function animate() {
    requestAnimationFrame(animate);
    if (controls3D) controls3D.update();
    if (vehicleGlow && vehicleGlow.visible) {
      const t = Date.now() * 0.003;
      vehicleGlow.material.opacity = 0.15 + Math.sin(t) * 0.15;
      const s = 1 + Math.sin(t) * 0.2; vehicleGlow.scale.set(s, s, s);
    }
    renderer3D.render(scene3D, camera3D);
  })();
}

function updatePosition(ecefX, ecefY, ecefZ, timestamp) {
  if (!originSet) { originX = ecefX; originY = ecefY; originZ = ecefZ; originSet = true; }
  const localX = ecefX - originX, localY = ecefZ - originZ, localZ = ecefY - originY;
  $('val-posX').textContent = ecefX.toFixed(2);
  $('val-posY').textContent = ecefY.toFixed(2);
  $('val-posZ').textContent = ecefZ.toFixed(2);
  $('val-altitude').textContent = ecefZ.toFixed(2) + ' m';
  if (positionHistory.length > 0) {
    const prev = positionHistory[positionHistory.length - 1];
    const dist = Math.sqrt((localX - prev.x) ** 2 + (localY - prev.y) ** 2 + (localZ - prev.z) ** 2);
    const dt = (timestamp - lastUpdateTime) / 1000;
    if (dt > 0 && dist > 0.01) {
      totalDistance += dist;
      $('val-groundSpeed').textContent = (dist / dt).toFixed(2) + ' m/s';
      $('val-totalDistance').textContent = totalDistance.toFixed(1) + ' m';
    }
  }
  positionHistory.push({ x: localX, y: localY, z: localZ });
  if (positionHistory.length > MAX_TRACK_POINTS) positionHistory.shift();
  lastPosition = { x: localX, y: localY, z: localZ }; lastUpdateTime = timestamp;
  if (trackGeometry && trackPointCount < MAX_TRACK_POINTS) {
    const i = trackPointCount * 3;
    trackPositions[i] = localX; trackPositions[i + 1] = localY; trackPositions[i + 2] = localZ;
    trackPointCount++;
    trackGeometry.attributes.position.needsUpdate = true;
    trackGeometry.setDrawRange(0, trackPointCount);
  }
  if (vehicleSphere) {
    vehicleSphere.position.set(localX, localY, localZ); vehicleSphere.visible = true;
    vehicleGlow.position.set(localX, localY + 0.05, localZ); vehicleGlow.visible = true;
  }
  const tp = $('val-trackPoints'); if (tp) tp.textContent = trackPointCount.toString();
  checkLapTrigger(localX, localY);
  updateLeafletMap(ecefX, ecefY, ecefZ);
}

// ═══════════════════════════════════════════════════════════
//  LEAFLET SATELLITE MAP
// ═══════════════════════════════════════════════════════════
const WGS84_A = 6378137.0, WGS84_F = 1 / 298.257223563;
const WGS84_B = WGS84_A * (1 - WGS84_F);
const WGS84_E2 = 1 - (WGS84_B * WGS84_B) / (WGS84_A * WGS84_A);
function ecefToLatLon(x, y, z) {
  const lon = Math.atan2(y, x), p = Math.sqrt(x * x + y * y);
  let lat = Math.atan2(z, p * (1 - WGS84_E2)), N, sinLat;
  for (let i = 0; i < 5; i++) { sinLat = Math.sin(lat); N = WGS84_A / Math.sqrt(1 - WGS84_E2 * sinLat * sinLat); lat = Math.atan2(z + WGS84_E2 * N * sinLat, p); }
  sinLat = Math.sin(lat); N = WGS84_A / Math.sqrt(1 - WGS84_E2 * sinLat * sinLat);
  return { lat: lat * (180 / Math.PI), lon: lon * (180 / Math.PI), alt: p / Math.cos(lat) - N };
}
let leafletMap = null, leafletTrack = null, leafletMarker = null, leafletTrackPoints = [], leafletInitialized = false;

function initLeafletMap() {
  const c = $('leafletMap'); if (!c || typeof L === 'undefined') return;
  leafletMap = L.map('leafletMap', { center: [12.87, 74.84], zoom: 18, zoomControl: true });
  L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}', { attribution: 'Tiles: Esri', maxZoom: 20 }).addTo(leafletMap);
  leafletTrack = L.polyline([], { color: '#ff0a0a', weight: 3, opacity: 0.9, smoothFactor: 1 }).addTo(leafletMap);
  const icon = L.divIcon({ className: '', html: '<div style="width:14px;height:14px;background:#ff0a0a;border-radius:50%;border:2px solid #fff;box-shadow:0 0 12px #ff0a0a,0 0 24px rgba(255,10,10,0.4);"></div>', iconSize: [14, 14], iconAnchor: [7, 7] });
  leafletMarker = L.marker([0, 0], { icon }).addTo(leafletMap); leafletMarker.setOpacity(0);
  leafletInitialized = true;
}

function updateLeafletMap(ecefX, ecefY, ecefZ) {
  if (!leafletInitialized || !leafletMap) return;
  if (ecefX === 0 && ecefY === 0 && ecefZ === 0) return;
  const { lat, lon } = ecefToLatLon(ecefX, ecefY, ecefZ);
  if (!isFinite(lat) || !isFinite(lon) || Math.abs(lat) > 90) return;
  const latEl = $('val-lat'), lonEl = $('val-lon');
  if (latEl) latEl.textContent = lat.toFixed(6);
  if (lonEl) lonEl.textContent = lon.toFixed(6);
  const latlng = L.latLng(lat, lon);
  if (leafletTrackPoints.length === 0) leafletMap.setView(latlng, 18);
  leafletTrackPoints.push(latlng);
  if (leafletTrackPoints.length > MAX_TRACK_POINTS) leafletTrackPoints.shift();
  leafletTrack.setLatLngs(leafletTrackPoints);
  leafletMarker.setLatLng(latlng); leafletMarker.setOpacity(1);
  if (!leafletMap.getBounds().contains(latlng)) leafletMap.panTo(latlng, { animate: true, duration: 0.5 });
}

// ═══════════════════════════════════════════════════════════
//  applyTelemetry — ONLY stores data, no DOM/canvas writes!
// ═══════════════════════════════════════════════════════════
function applyTelemetry(d) {
  pendingChartData = d;   // overwrite with latest — we only want the newest
  scheduleRAF();
}

// ═══════════════════════════════════════════════════════════
//  PARSER 1 — Binary 79-byte framed packets
// ═══════════════════════════════════════════════════════════
function tryBinaryPackets() {
  let i = 0, found = 0;
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
      if (isFinite(d.bmsTotal) && isFinite(d.yaw) && isFinite(d.motorSpeed) && d.bmsTotal < 600) {
        applyTelemetry(d);
        addLog(`[BIN] ✓ T=${d.timestamp}ms V=${d.bmsTotal.toFixed(1)}V Motor=${d.motorSpeed.toFixed(0)}RPM VSM=${d.vsmState}`, 'ok');
        found++;
      }
      i += PACKET_SIZE;
    } else { i++; }
  }
  if (i > 0) { rxBuf.copyWithin(0, i, rxLen); rxLen -= i; }
  return found > 0;
}

// ═══════════════════════════════════════════════════════════
//  PARSER 2 — CSV
// ═══════════════════════════════════════════════════════════
function tryParseCSV(rawLine) {
  let line = rawLine.trim(); if (!line) return false;
  const arrow = line.indexOf(' -> '); if (arrow !== -1) line = line.substring(arrow + 4).trim();
  for (const p of ['DATA:', 'TXT:', 'PKT:']) { if (line.startsWith(p)) { line = line.substring(p.length).trim(); break; } }
  if (/^(Qlen|This ESP|Ready|Peer|Teensy|==|ESP-NOW|Warning|ERROR|Queue full|Stats|Parsed|Created|Web server|System|Listening|MAC|Access|Logged|SPIFFS)/.test(line)) return false;
  const parts = line.split(','); if (parts.length < 14) return false;
  const vals = parts.map(Number); if (vals.slice(0, 5).some(isNaN)) return false;
  if (vals[1] < 0 || vals[1] > 600) return false;
  const t = {
    timestamp: vals[0], bmsTotal: vals[1], bmsPackAvgTemp: vals[2], bmsCurrent: vals[3], bmsLowestCellV: vals[4],
    motorSpeed: vals[5], segVolt0: vals[6] || 0, segVolt1: vals[7] || 0, segVolt2: vals[8] || 0, segVolt3: vals[9] || 0, segVolt4: vals[10] || 0,
    vsmState: vals[11] || 0, posX: vals[12] || 0, posY: vals[13] || 0, posZ: vals[14] || 0,
    wheelSpeed: vals[15] || 0, vehicleSpeed: vals[16] || 0,
    yaw: vals[17] || 0, pitch: vals[18] || 0, roll: vals[19] || 0,
  };
  if (parts.length >= 14 && parts.length < 20) { t.yaw = vals[11] || 0; t.pitch = vals[12] || 0; t.roll = vals[13] || 0; t.vsmState = 0; t.posX = 0; t.posY = 0; t.posZ = 0; t.wheelSpeed = 0; t.vehicleSpeed = 0; }
  applyTelemetry(t);
  addLog(`[CSV] ✓ T=${vals[0]}ms V=${vals[1].toFixed(1)}V Motor=${vals[5].toFixed(0)}RPM`, 'ok');
  return true;
}

// ═══════════════════════════════════════════════════════════
//  PARSER 3 — JSON
// ═══════════════════════════════════════════════════════════
function tryParseJSON(line) {
  const start = line.indexOf('{'); if (start === -1) return false;
  const end = line.lastIndexOf('}'); if (end === -1) return false;
  try {
    const o = JSON.parse(line.substring(start, end + 1));
    const bv = o.bmsTotal || o.bmsV || o.V || o.voltage || 0;
    if (!isFinite(bv)) return false;
    applyTelemetry({
      timestamp: o.timestamp || o.ts || 0, bmsTotal: bv,
      bmsPackAvgTemp: o.bmsPackAvgTemp || o.bmsAvgTemp || o.bmsTemp || o.T || 0,
      bmsCurrent: o.bmsCurrent || o.bmsCur || o.I || 0,
      bmsLowestCellV: o.bmsLowestCellV || o.bmsLowCell || 0,
      yaw: o.yaw || 0, pitch: o.pitch || 0, roll: o.roll || 0,
      motorSpeed: o.motorSpeed || 0, wheelSpeed: o.wheelSpeed || 0, vehicleSpeed: o.vehicleSpeed || 0,
      segVolt0: o.segVolt0 || 0, segVolt1: o.segVolt1 || 0, segVolt2: o.segVolt2 || 0,
      segVolt3: o.segVolt3 || 0, segVolt4: o.segVolt4 || 0,
      vsmState: o.vsmState || 0, posX: o.posX || 0, posY: o.posY || 0, posZ: o.posZ || 0,
    });
    addLog(`[JSON] T=${o.timestamp || 0}ms V=${bv.toFixed(1)}V`, 'ok');
    return true;
  } catch (_) { return false; }
}

// ── Main chunk handler ─────────────────────────────────────
const textDec = new TextDecoder('utf-8', { fatal: false });
function handleChunk(bytes) {
  for (const b of bytes) {
    if (rxLen < rxBuf.length) rxBuf[rxLen++] = b;
    else { rxBuf.copyWithin(0, 256); rxLen -= 256; rxBuf[rxLen++] = b; }
  }
  tryBinaryPackets();
  lineBuf += textDec.decode(bytes, { stream: true });
  const lines = lineBuf.split('\n');
  lineBuf = lines.pop();
  for (const raw of lines) {
    const line = raw.replace(/\r/g, '').trim(); if (!line) continue;
    addLog(`← ${line.length > 80 ? line.substring(0, 80) + '…' : line}`);
    tryParseJSON(line) || tryParseCSV(line);
  }
}

// ── WebSocket (ESP32 AP) ───────────────────────────────────
function connectWiFi() {
  if (ws) ws.close();
  const host = location.hostname === '192.168.4.1' ? '192.168.4.1' : location.hostname || '192.168.4.1';
  setStatus('connecting');
  addLog(`Attempting WebSocket connection to ws://${host}:81...`, 'ok');

  ws = new WebSocket(`ws://${host}:81`);
  ws.binaryType = 'arraybuffer';

  ws.onopen = () => {
    isWsConnected = true;
    rxLen = 0; lineBuf = '';
    packetCount = 0; rateCounter = 0; rateLastTime = Date.now();
    setStatus('ws-connected');
    addLog(`WiFi Connected to ${host}! Receiving live binary telemetry.`, 'ok');
  };

  ws.onmessage = (e) => {
    if (e.data instanceof ArrayBuffer) {
      handleChunk(new Uint8Array(e.data));
    }
  };

  ws.onclose = () => {
    isWsConnected = false;
    if (!isConnected && !demoMode) setStatus('disconnected');
    addLog('WiFi Disconnected. Reconnecting in 3s...', 'err');
    setTimeout(connectWiFi, 3000);
  };

  ws.onerror = (err) => {
    if (!isConnected && !demoMode && !isWsConnected) {
      setStatus('disconnected');
    }
  };
}

// ── Web Serial ─────────────────────────────────────────────
async function connect() {
  try {
    if (!('serial' in navigator)) { alert('Web Serial API not supported!\nPlease use Chrome or Edge 89+.'); return; }
    setStatus('connecting');
    port = await navigator.serial.requestPort();
    const baud = parseInt($('baudSelect').value, 10);
    await port.open({ baudRate: baud });
    isConnected = true; rxLen = 0; lineBuf = '';
    packetCount = 0; rateCounter = 0; rateLastTime = Date.now();
    setStatus('connected');
    addLog(`Connected @ ${baud} baud — watching for binary/CSV/JSON…`, 'ok');
    readLoop();
  } catch (err) {
    setStatus('disconnected');
    if (err.name !== 'NotFoundError') addLog('Error: ' + err.message, 'err');
  }
}
async function disconnect() {
  isConnected = false;
  try { if (reader) await reader.cancel(); } catch (_) { }
  try { if (port) await port.close(); } catch (_) { }
  reader = null; port = null;
  setStatus('disconnected');
  addLog(`Disconnected — ${packetCount} packets decoded`, 'err');
  $('dataRate').textContent = '- Hz';
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
    if (isConnected) addLog('Read error: ' + err.message, 'err');
  } finally {
    reader = null; if (isConnected) disconnect();
  }
}

// ── Button handlers ────────────────────────────────────────
async function toggleConnect() { if (demoMode) stopDemo(); if (isConnected) disconnect(); else connect(); }
function toggleDemo() { demoMode ? stopDemo() : startDemo(); }
function toggleDiag() { addLog('Note: all incoming lines shown in log.', 'ok'); }

function startDemo() {
  if (isConnected) disconnect();
  demoMode = true; demoT = 0;
  $('demoBtn').classList.add('active');
  setStatus('demo');
  addLog('Demo mode ON — synthetic data @ 50 Hz', 'ok');
  demoTimer = setInterval(() => {
    demoT += 0.02;
    const t = demoT;
    const motorRPM = 3000 + Math.sin(t * 0.4) * 1500;
    const wheelRPM = motorRPM / 5.818;
    const vehicleKmh = wheelRPM * 1.435 * 60 / 1000;
    const trackRadius = 100;
    applyTelemetry({
      timestamp: Math.round(t * 1000),
      bmsTotal: 90 + Math.sin(t * 0.3) * 15,
      bmsPackAvgTemp: 35 + Math.sin(t * 0.2) * 10,
      bmsCurrent: 80 + Math.sin(t * 0.7) * 60,
      bmsLowestCellV: 3.7 + Math.sin(t * 0.15) * 0.2,
      motorSpeed: motorRPM,
      segVolt0: 72 + Math.sin(t * 0.3) * 3, segVolt1: 71 + Math.sin(t * 0.32) * 3,
      segVolt2: 73 + Math.sin(t * 0.34) * 3, segVolt3: 70 + Math.sin(t * 0.28) * 3,
      segVolt4: 72 + Math.sin(t * 0.36) * 3,
      vsmState: t > 2 ? 6 : (t > 1 ? 4 : (t > 0.5 ? 2 : 0)),
      posX: 1341900 + Math.sin(t * 0.15) * trackRadius,
      posY: 5765700 + Math.cos(t * 0.15) * trackRadius * 0.6,
      posZ: 1410900 + Math.sin(t * 0.05) * 3,
      wheelSpeed: wheelRPM, vehicleSpeed: vehicleKmh,
      yaw: (t * 30) % 360 - 180, pitch: Math.sin(t * 0.5) * 20, roll: Math.sin(t * 0.8) * 25,
    });
  }, 20); // 50Hz demo
}
function stopDemo() {
  clearInterval(demoTimer); demoMode = false;
  $('demoBtn').classList.remove('active');
  setStatus('disconnected');
  addLog('Demo mode OFF');
  $('dataRate').textContent = '- Hz';
}

// ── Log (non-blocking — just queues) ──────────────────────
function addLog(msg, type = '') {
  logQueue.push({ msg, type });
  if (logQueue.length > 200) logQueue.splice(0, logQueue.length - 200); // cap
  scheduleRAF();
}
function clearLog() { $('logBody').innerHTML = ''; logQueue = []; }

// ── Init ──────────────────────────────────────────────────
(function init() {
  addLog('Ready — connect or use DEMO to see data.', 'ok');
  if (!('serial' in navigator)) addLog('Web Serial not available — use Chrome or Edge 89+.', 'err');
  init3DMap();

  // Initialize Google Maps instead of Leaflet
  if (typeof initGoogleMap === 'function' && typeof google !== 'undefined') {
    initGoogleMap();
  } else if (typeof initLeafletMap === 'function') {
    initLeafletMap();
  }

  // Auto-connect to WiFi WebSocket if we are loaded from the ESP32 AP
  if (location.hostname === '192.168.4.1' || location.protocol === 'http:') {
    connectWiFi();
  }
})();
</script>
</body>

</html>
)HTMLEOF";
