#pragma once

#include "InputDevice.h"
#include "Model.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <algorithm>
#ifdef ESP32
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

namespace Espfc::Device {

struct FlightLogEntry
{
  uint32_t timeMs;
  int16_t heightCm;
  int16_t targetCm;
  int16_t laserCm;
  int16_t varioCms;
  int16_t rollDeg;
  int16_t pitchDeg;
  int16_t stickThr;
  int16_t outThr;
  int16_t pTerm;
  int16_t iTerm;
  int16_t dTerm;
  uint8_t flags; // bit 0: armed, bit 1: altHold, bit 2: laserValid
};

const char JOYSTICK_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>ESP32 Drone Cockpit</title>
<style>
  :root {
    --bg: #0a0e1a;
    --card: rgba(255,255,255,0.06);
    --border: rgba(255,255,255,0.12);
    --accent: #00e5ff;
    --green: #00e676;
    --red: #ff1744;
    --amber: #ffab00;
    --text: #e0e0e0;
    --sub: #8e99a4;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-user-select: none; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; touch-action: none; }
  body { 
    background: linear-gradient(135deg, #0a0e1a 0%, #141b2d 100%); 
    color: var(--text); 
    height: 100vh; 
    width: 100vw; 
    display: flex; 
    flex-direction: column; 
    overflow: hidden; 
  }

  .hud-container { display: flex; flex-direction: column; height: 100%; padding: 6px; }

  /* Top Status Bar */
  .status-bar { display: flex; justify-content: space-between; align-items: center; padding: 6px 12px; background: var(--card); backdrop-filter: blur(12px); border: 1px solid var(--border); border-radius: 12px; margin-bottom: 6px; height: 42px; font-weight: 600; font-size: 0.78rem; white-space: nowrap; }
  .status-group { display: flex; align-items: center; gap: 12px; }
  
  .btn-hud { background: var(--card); color: var(--accent); border: 1px solid var(--border); border-radius: 8px; font-family: inherit; font-size: 0.75rem; font-weight: 700; padding: 4px 10px; cursor: pointer; text-decoration: none; backdrop-filter: blur(8px); transition: all 0.15s ease; }
  .btn-hud:active { background: var(--accent); color: var(--bg); }
  
  #btn-arm { color: var(--red); border-color: rgba(255,23,68,0.4); }
  #btn-arm.armed { color: var(--green); border-color: rgba(0,230,118,0.4); animation: pulse 1.5s infinite; }
  @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
  
  #btn-alt { color: var(--amber); border-color: rgba(255,171,0,0.4); }
  #btn-alt.active { color: var(--green); border-color: rgba(0,230,118,0.4); background: rgba(0,230,118,0.1); }

  /* Flight Deck (Main Joysticks) */
  .flight-deck { flex: 1; display: flex; justify-content: space-between; align-items: center; padding: 4px 14px; gap: 10px; }
  
  .stick-col { display: flex; flex-direction: column; align-items: center; gap: 6px; }
  .stick-lbl { font-size: 0.72rem; font-weight: 700; color: var(--sub); text-transform: uppercase; letter-spacing: 0.5px; }
  
  .stick-zone { 
    width: 150px; height: 150px; 
    border-radius: 50%; 
    border: 1px solid var(--border);
    background: var(--card);
    backdrop-filter: blur(8px);
    position: relative; 
    display: flex; justify-content: center; align-items: center;
  }
  .stick-zone::before { content: ""; position: absolute; width: 100%; height: 1px; background: rgba(255,255,255,0.08); }
  .stick-zone::after { content: ""; position: absolute; height: 100%; width: 1px; background: rgba(255,255,255,0.08); }
  
  .stick-knob { 
    width: 50px; height: 50px; 
    border-radius: 50%; 
    border: 2px solid var(--accent);
    background: rgba(0,229,255,0.08);
    box-shadow: 0 0 12px rgba(0,229,255,0.2);
    position: absolute; 
    display: flex; justify-content: center; align-items: center;
    pointer-events: none;
  }
  .stick-knob::after { content: "+"; color: var(--accent); font-size: 20px; line-height: 0; font-weight: 300; }

  /* Center Column */
  .center-col { display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 8px; flex: 1; min-width: 250px; }
  
  .trim-panel { width: 100%; background: var(--card); backdrop-filter: blur(8px); border: 1px solid var(--border); border-radius: 10px; padding: 8px; display: flex; flex-direction: column; gap: 6px; font-size: 0.72rem; font-weight: 600; }
  .trim-row { display: flex; justify-content: space-between; align-items: center; }
  .trim-grp { display: flex; align-items: center; gap: 4px; color: var(--sub); }
  .btn-t { background: var(--card); color: var(--accent); border: 1px solid var(--border); border-radius: 6px; width: 24px; height: 22px; display: flex; align-items: center; justify-content: center; cursor: pointer; font-size: 0.75rem; font-weight: 700; }
  .btn-t:active { background: var(--accent); color: var(--bg); }
  .trim-v { min-width: 28px; text-align: center; color: var(--text); }

  /* Telemetry Strip */
  .tele-strip { display: flex; justify-content: space-between; align-items: center; padding: 6px 12px; background: var(--card); backdrop-filter: blur(12px); border: 1px solid var(--border); border-radius: 12px; margin-top: 6px; font-size: 0.72rem; font-weight: 600; white-space: nowrap; color: var(--sub); }

  /* Bottom Sheet Modal */
  .pid-modal { 
    position: fixed; bottom: -100%; left: 0; width: 100%; 
    background: linear-gradient(180deg, #0a0e1a 0%, #141b2d 100%); 
    border-top: 1px solid var(--border); border-radius: 16px 16px 0 0;
    transition: bottom 0.3s ease; z-index: 1000;
    max-height: 95vh; display: flex; flex-direction: column;
    touch-action: pan-y !important;
  }
  .pid-modal.open { bottom: 0; }
  .pid-box { 
    padding: 10px 12px; overflow-y: auto; flex: 1; display: flex; flex-direction: column; gap: 8px;
    touch-action: pan-y !important; -webkit-overflow-scrolling: touch;
  }
  .pid-hdr { display: flex; justify-content: space-between; border-bottom: 1px solid var(--border); padding-bottom: 6px; font-size: 0.85rem; font-weight: 700; }
  
  .pid-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 8px; }
  .pid-card { background: var(--card); border: 1px solid var(--border); border-radius: 10px; padding: 6px 8px; display: flex; flex-direction: column; gap: 4px; }
  .pid-title { font-size: 0.72rem; font-weight: 700; color: var(--accent); border-bottom: 1px solid var(--border); padding-bottom: 3px; margin-bottom: 2px; text-transform: uppercase; letter-spacing: 0.5px; }
  .pid-row { display: flex; justify-content: space-between; align-items: center; font-size: 0.72rem; }
  .pid-row input { background: rgba(255,255,255,0.05); border: 1px solid var(--border); border-radius: 6px; color: var(--text); font-family: inherit; width: 44px; text-align: center; font-size: 0.72rem; font-weight: 700; padding: 2px; }
  
  .pid-status { text-align: center; font-size: 0.72rem; min-height: 18px; background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 4px; color: var(--sub); }
  .pid-actions { display: flex; gap: 8px; }
  .pid-actions button { flex: 1; padding: 8px; border-radius: 8px; }

  /* Mobile Landscape Layout: 4 columns so all tuning cards fit on screen */
  @media (orientation: landscape) and (max-height: 520px) {
    .pid-modal { height: 100vh; max-height: 100vh; border-radius: 0; }
    .pid-box { padding: 4px 10px; gap: 4px; }
    .pid-hdr { padding-bottom: 3px; font-size: 0.75rem; }
    .pid-grid { grid-template-columns: repeat(4, 1fr); gap: 6px; }
    .pid-card { padding: 4px 6px; gap: 2px; border-radius: 8px; }
    .pid-title { font-size: 0.65rem; padding-bottom: 2px; margin-bottom: 1px; }
    .pid-row { font-size: 0.65rem; }
    .pid-row input { width: 36px; padding: 1px; font-size: 0.65rem; }
    .btn-t { width: 20px; height: 18px; font-size: 0.65rem; }
    .pid-status { min-height: 14px; font-size: 0.65rem; padding: 2px; }
    .pid-actions button { padding: 5px; font-size: 0.72rem; }
  }

</style>
</head>
<body>
<div class="hud-container">
  
  <div class="status-bar">
    <div class="status-group">
      <span style="color:var(--accent);">✈ FC-01</span>
      <span id="hud-bat-icon" style="color:var(--sub);">BAT</span><span id="hud-bat-val">0.00V 0%</span>
      <span id="hud-att" style="color:var(--sub);">R:0.0° P:0.0°</span>
      <span>ALT <span id="hud-alt" style="color:var(--accent);">0cm</span></span>
    </div>
    <div class="status-group">
      <span id="hud-sensor" style="color:var(--sub);">ESTIMATING</span>
      <button class="btn-hud" id="btn-arm">DISARMED</button>
      <button class="btn-hud" id="btn-fs" onclick="toggleFullScreen()">⛶</button>
      <button class="btn-hud" onclick="openPidModal()">⚙ TUNE</button>
    </div>
  </div>

  <div class="flight-deck">
    <div class="stick-col">
      <div class="stick-lbl" id="lbl-left">THR / YAW</div>
      <div class="stick-zone" id="zone-left">
        <div class="stick-knob" id="knob-left"></div>
      </div>
    </div>

    <div class="center-col">
      <div style="display:flex; gap:8px;">
        <button class="btn-hud" id="btn-alt">ALT HOLD</button>
        <button class="btn-hud" id="btn-yaw-toggle">YAW: ON</button>
        <button class="btn-hud" id="btn-cal" onclick="calibrateFlat()">CAL</button>
      </div>

      <div class="trim-panel">
        <div class="trim-row">
          <div class="trim-grp">TRIM R: <span class="trim-v" id="t-r">0</span></div>
          <div class="trim-grp">P: <span class="trim-v" id="t-p">0</span></div>
          <div class="trim-grp">Y: <span class="trim-v" id="t-y">0</span></div>
          <button class="btn-hud" style="color:var(--red);border-color:rgba(255,23,68,0.4);" onclick="rstTrim()">CLR</button>
        </div>
        <div class="trim-row">
          <div class="trim-grp">RANGE: ±<span class="trim-v" id="stick-max-val">500</span></div>
          <div class="trim-grp">
            <button class="btn-t" onclick="adjStickMax(-50)">-</button>
            <button class="btn-t" onclick="adjStickMax(50)">+</button>
          </div>
        </div>
      </div>
      
      <button class="btn-hud" style="color:var(--red);border-color:rgba(255,23,68,0.4);width:100%;padding:6px;font-size:0.8rem;" id="btn-kill">⚠ EMERGENCY CUT</button>
    </div>

    <div class="stick-col">
      <div class="stick-lbl">PITCH / ROLL</div>
      <div class="stick-zone" id="zone-right">
        <div class="stick-knob" id="knob-right"></div>
      </div>
      <div class="stick-lbl" id="lbl-right-range" style="font-size:0.6rem;">1000-2000</div>
    </div>
  </div>

  <div class="tele-strip">
    <span>THR: <span id="disp-thr" style="color:var(--text);">1000</span></span>
    <span>YAW: <span id="disp-yaw" style="color:var(--text);">1500</span></span>
    <span>PIT: <span id="disp-pitch" style="color:var(--text);">1500</span></span>
    <span>ROL: <span id="disp-roll" style="color:var(--text);">1500</span></span>
    <span>HDG: <span id="hud-yaw" style="color:var(--text);">0°</span></span>
    <span style="display:none;" id="disp-bat"></span>
    <button class="btn-hud" onclick="exportPhoneCSV()">DL LOG</button>
    <span id="badge" style="color:var(--accent);">WS:50Hz</span>
  </div>
</div>

<div id="modal-pid" class="pid-modal">
  <div class="pid-box">
    <div class="pid-hdr">
      <span>⚙ PID Tuning</span>
      <button class="btn-hud" onclick="closePidModal()">Close</button>
    </div>
    
    <div class="pid-grid">
      <div class="pid-card">
        <div class="pid-title">Roll</div>
        <div class="pid-row"><span>P</span><button class="btn-t" onclick="stepPid('rp',-1)">-</button><input type="number" id="inp-rp" value="46"><button class="btn-t" onclick="stepPid('rp',1)">+</button></div>
        <div class="pid-row"><span>I</span><button class="btn-t" onclick="stepPid('ri',-2)">-</button><input type="number" id="inp-ri" value="70"><button class="btn-t" onclick="stepPid('ri',2)">+</button></div>
        <div class="pid-row"><span>D</span><button class="btn-t" onclick="stepPid('rd',-1)">-</button><input type="number" id="inp-rd" value="24"><button class="btn-t" onclick="stepPid('rd',1)">+</button></div>
        <div class="pid-row"><span>F</span><button class="btn-t" onclick="stepPid('rf',-2)">-</button><input type="number" id="inp-rf" value="30"><button class="btn-t" onclick="stepPid('rf',2)">+</button></div>
      </div>
      
      <div class="pid-card">
        <div class="pid-title">Pitch</div>
        <div class="pid-row"><span>P</span><button class="btn-t" onclick="stepPid('pp',-1)">-</button><input type="number" id="inp-pp" value="50"><button class="btn-t" onclick="stepPid('pp',1)">+</button></div>
        <div class="pid-row"><span>I</span><button class="btn-t" onclick="stepPid('pi',-2)">-</button><input type="number" id="inp-pi" value="75"><button class="btn-t" onclick="stepPid('pi',2)">+</button></div>
        <div class="pid-row"><span>D</span><button class="btn-t" onclick="stepPid('pd',-1)">-</button><input type="number" id="inp-pd" value="24"><button class="btn-t" onclick="stepPid('pd',1)">+</button></div>
        <div class="pid-row"><span>F</span><button class="btn-t" onclick="stepPid('pf',-2)">-</button><input type="number" id="inp-pf" value="30"><button class="btn-t" onclick="stepPid('pf',2)">+</button></div>
      </div>
      
      <div class="pid-card">
        <div class="pid-title">Yaw</div>
        <div class="pid-row"><span>P</span><button class="btn-t" onclick="stepPid('yp',-2)">-</button><input type="number" id="inp-yp" value="72"><button class="btn-t" onclick="stepPid('yp',2)">+</button></div>
        <div class="pid-row"><span>I</span><button class="btn-t" onclick="stepPid('yi',-2)">-</button><input type="number" id="inp-yi" value="50"><button class="btn-t" onclick="stepPid('yi',2)">+</button></div>
        <div class="pid-row"><span>D</span><button class="btn-t" onclick="stepPid('yd',-1)">-</button><input type="number" id="inp-yd" value="0"><button class="btn-t" onclick="stepPid('yd',1)">+</button></div>
        <div class="pid-row"><span>F</span><button class="btn-t" onclick="stepPid('yf',-2)">-</button><input type="number" id="inp-yf" value="30"><button class="btn-t" onclick="stepPid('yf',2)">+</button></div>
      </div>
      
      <div class="pid-card">
        <div class="pid-title">Level / Alt / Bat</div>
        <div class="pid-row"><span>LVL P</span><button class="btn-t" onclick="stepPid('lp',-2)">-</button><input type="number" id="inp-lp" value="65"><button class="btn-t" onclick="stepPid('lp',2)">+</button></div>
        <div class="pid-row"><span>ALT P</span><button class="btn-t" onclick="stepPid('vp',-2)">-</button><input type="number" id="inp-vp" value="38"><button class="btn-t" onclick="stepPid('vp',2)">+</button></div>
        <div class="pid-row"><span>ALT I</span><button class="btn-t" onclick="stepPid('vi',-2)">-</button><input type="number" id="inp-vi" value="18"><button class="btn-t" onclick="stepPid('vi',2)">+</button></div>
        <div class="pid-row"><span>BAT SC</span><button class="btn-t" onclick="stepPid('vs',-1)">-</button><input type="number" id="inp-vs" value="112"><button class="btn-t" onclick="stepPid('vs',1)">+</button></div>
      </div>
    </div>
    
    <div id="pid-status" class="pid-status">Ready</div>
    <div class="pid-actions">
      <button class="btn-hud" onclick="applyPidsLive()">⚡ Apply Live</button>
      <button class="btn-hud" onclick="savePidsEEPROM()">💾 Save EEPROM</button>
      <button class="btn-hud" onclick="fetchPids()">🔄 Reload</button>
    </div>
  </div>
</div>

<script>
  function toggleFullScreen() {
    const doc = document.documentElement;
    if (!document.fullscreenElement && !document.webkitFullscreenElement) {
      if (doc.requestFullscreen) doc.requestFullscreen();
      else if (doc.webkitRequestFullscreen) doc.webkitRequestFullscreen();
    } else {
      if (document.exitFullscreen) document.exitFullscreen();
      else if (document.webkitExitFullscreen) document.webkitExitFullscreen();
    }
  }

  let isArmed = false;
  let isAltHold = false;
  let leftStickRecenterY = false;
  let allowYaw = true;
  let roll = 1500, pitch = 1500, throttle = 1000, yaw = 1500;
  let stickMaxRange = 500;

  function adjStickMax(delta) {
    stickMaxRange = Math.max(50, Math.min(500, stickMaxRange + delta));
    document.getElementById('stick-max-val').innerText = stickMaxRange;
    document.getElementById('lbl-right-range').innerText = (1500 - stickMaxRange) + '-' + (1500 + stickMaxRange);
  }

  const knobLeft = document.getElementById('knob-left');
  const btnYaw = document.getElementById('btn-yaw-toggle');
  btnYaw.addEventListener('click', () => {
    allowYaw = !allowYaw;
    if (allowYaw) {
      btnYaw.innerText = 'YAW: ON';
      document.getElementById('lbl-left').innerText = 'THR / YAW';
    } else {
      btnYaw.innerText = 'YAW: OFF';
      document.getElementById('lbl-left').innerText = 'THR ONLY';
      yaw = 1500;
      document.getElementById('disp-yaw').innerText = 1500;
      const match = knobLeft.style.transform.match(/translate\((.+)px,\s*(.+)px\)/);
      const curY = match ? match[2] : (leftStickRecenterY ? "0" : "50");
      knobLeft.style.transform = `translate(0px, ${curY}px)`;
    }
  });

  // Left Stick (8% deadband on X)
  setupJoystick(document.getElementById('zone-left'), knobLeft, (x, y) => {
    let effX = Math.abs(x) < 0.08 ? 0 : (x > 0 ? (x - 0.08) / 0.92 : (x + 0.08) / 0.92);
    yaw = allowYaw ? Math.round(1500 + effX * 500) : 1500;
    throttle = Math.round(1500 - y * 500);
    throttle = Math.max(1000, Math.min(2000, throttle));
    document.getElementById('disp-thr').innerText = throttle;
    document.getElementById('disp-yaw').innerText = yaw;
  }, false, true);

  // Right Stick (5% deadband on both X and Y - NEW FIX)
  setupJoystick(document.getElementById('zone-right'), document.getElementById('knob-right'), (x, y) => {
    let effX = Math.abs(x) < 0.05 ? 0 : (x > 0 ? (x - 0.05) / 0.95 : (x + 0.05) / 0.95);
    let effY = Math.abs(y) < 0.05 ? 0 : (y > 0 ? (y - 0.05) / 0.95 : (y + 0.05) / 0.95);
    roll = Math.round(1500 + effX * stickMaxRange);
    pitch = Math.round(1500 - effY * stickMaxRange);
    document.getElementById('disp-roll').innerText = roll;
    document.getElementById('disp-pitch').innerText = pitch;
  }, true, false);

  function setupJoystick(zone, knob, onMove, recenterY, isLeftStick) {
    let activeTouchId = null;
    const maxRadius = 50;

    function handleMove(clientX, clientY) {
      const rect = zone.getBoundingClientRect();
      const centerX = rect.left + rect.width / 2;
      const centerY = rect.top + rect.height / 2;
      let dx = (isLeftStick && !allowYaw) ? 0 : (clientX - centerX);
      let dy = clientY - centerY;
      let dist = Math.hypot(dx, dy);
      if (dist > maxRadius) {
        if (isLeftStick && !allowYaw) {
          dy = Math.sign(dy) * maxRadius;
        } else {
          dx = (dx / dist) * maxRadius;
          dy = (dy / dist) * maxRadius;
        }
      }
      knob.style.transform = `translate(${dx}px, ${dy}px)`;
      onMove(dx / maxRadius, dy / maxRadius);
    }

    function resetStick() {
      if (recenterY || (isLeftStick && leftStickRecenterY)) {
        knob.style.transform = `translate(0px, 0px)`;
        onMove(0, 0);
      } else {
        const match = knob.style.transform.match(/translate\((.+)px,\s*(.+)px\)/);
        const curY = match ? match[2] : "50";
        knob.style.transform = `translate(0px, ${curY}px)`;
        onMove(0, parseFloat(curY) / maxRadius);
      }
    }

    zone.addEventListener('touchstart', e => {
      e.preventDefault();
      const touch = e.changedTouches[0];
      activeTouchId = touch.identifier;
      handleMove(touch.clientX, touch.clientY);
    }, { passive: false });

    window.addEventListener('touchmove', e => {
      for (let t of e.changedTouches) {
        if (t.identifier === activeTouchId) {
          e.preventDefault();
          handleMove(t.clientX, t.clientY);
        }
      }
    }, { passive: false });

    window.addEventListener('touchend', e => {
      for (let t of e.changedTouches) {
        if (t.identifier === activeTouchId) {
          activeTouchId = null;
          resetStick();
        }
      }
    }, { passive: false });

    let isMouseDown = false;
    zone.addEventListener('mousedown', e => { isMouseDown = true; handleMove(e.clientX, e.clientY); });
    window.addEventListener('mousemove', e => { if (isMouseDown) handleMove(e.clientX, e.clientY); });
    window.addEventListener('mouseup', () => { if (isMouseDown) { isMouseDown = false; resetStick(); } });

    window.addEventListener('touchcancel', e => {
      for (const t of e.changedTouches) {
        if (t.identifier === activeTouchId) { activeTouchId = null; resetStick(); }
      }
    }, { passive: false });
    
    const hardReset = () => {
      if (activeTouchId !== null || isMouseDown) { activeTouchId = null; isMouseDown = false; resetStick(); }
    };
    window.addEventListener('blur', hardReset);
    document.addEventListener('visibilitychange', () => { if (document.hidden) hardReset(); });
  }

  const btnArm = document.getElementById('btn-arm');
  function updateArmUI() {
    btnArm.innerText = isArmed ? '>>> ARMED <<<' : 'DISARMED';
    btnArm.className = 'btn-hud' + (isArmed ? ' armed' : '');
  }
  btnArm.addEventListener('click', () => {
    isArmed = !isArmed;
    updateArmUI();
  });

  const btnAlt = document.getElementById('btn-alt');
  function updateAltUI() {
    btnAlt.className = 'btn-hud' + (isAltHold ? ' active' : '');
  }
  btnAlt.addEventListener('click', () => {
    isAltHold = !isAltHold;
    leftStickRecenterY = isAltHold;
    updateAltUI();
    
    if (isAltHold) {
      knobLeft.style.transform = 'translate(0px, 0px)';
      throttle = 1500;
    } else {
      knobLeft.style.transform = 'translate(0px, 50px)';
      throttle = 1000;
    }
    document.getElementById('disp-thr').innerText = throttle;
  });

  document.getElementById('btn-kill').addEventListener('click', () => {
    isArmed = false;
    throttle = 1000;
    updateArmUI();
    if(isAltHold) {
      isAltHold = false;
      leftStickRecenterY = false;
      updateAltUI();
      knobLeft.style.transform = 'translate(0px, 50px)';
    }
  });

  let ws = null;
  let wsConnected = false;
  let phoneFlightLog = [];
  let wasArmedPrev = false;

  function exportPhoneCSV() {
    if (phoneFlightLog.length === 0) {
      const stored = localStorage.getItem('drone_last_flight');
      if (stored) phoneFlightLog = JSON.parse(stored);
    }
    if (phoneFlightLog.length === 0) {
      alert('NO FLIGHT DATA');
      return;
    }
    let csv = "time_ms,height_cm,vario_cms,laser_lock,roll_deg,pitch_deg,heading_deg,stick_thr,stick_yaw,stick_pitch,stick_roll,m1_pct,m2_pct,m3_pct,m4_pct,armed,althold\n";
    const t0 = phoneFlightLog[0].t;
    phoneFlightLog.forEach(f => {
      csv += `${Math.round(f.t - t0)},${f.h},${f.v},${f.lv},${f.r},${f.p},${f.y},${f.thr},${f.yaw},${f.pitch},${f.roll},${f.m1},${f.m2},${f.m3},${f.m4},${f.arm},${f.alt}\n`;
    });
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `drone_log_${Date.now()}.csv`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function initWebSocket() {
    const wsUrl = 'ws://' + (location.hostname || '192.168.4.1') + ':81/';
    try {
      ws = new WebSocket(wsUrl);
      ws.binaryType = 'arraybuffer';
      ws.onopen = () => {
        wsConnected = true;
        document.getElementById('badge').innerText = 'WS:50HZ';
      };
      ws.onclose = () => {
        wsConnected = false;
        document.getElementById('badge').innerText = 'HTTP:33HZ';
        setTimeout(initWebSocket, 1000);
      };
      ws.onmessage = (event) => {
        try {
          if (typeof event.data === 'string') {
            const d = JSON.parse(event.data);
            
            if (d.vb !== undefined) {
              const pct = d.vp !== undefined ? d.vp : Math.max(0, Math.min(100, Math.round((d.vb - 3.3) / (4.2 - 3.3) * 100)));
              document.getElementById('hud-bat-val').innerText = `${d.vb.toFixed(2)}V ${pct}%`;
              if (d.vb < 3.5) document.getElementById('hud-bat-val').style.color = 'var(--red)';
              else if (d.vb < 3.7) document.getElementById('hud-bat-val').style.color = 'var(--amber)';
              else document.getElementById('hud-bat-val').style.color = 'var(--green)';
            }

            document.getElementById('hud-att').innerText = `R:${d.r.toFixed(1)}° P:${d.p.toFixed(1)}°`;
            document.getElementById('hud-yaw').innerText = `${d.y.toFixed(0)}°`;
            document.getElementById('hud-alt').innerText = `${d.h}cm`;

            const sensorEl = document.getElementById('hud-sensor');
            if (d.lv) {
              sensorEl.innerText = '● LASER LOCK';
              sensorEl.style.color = 'var(--green)';
            } else {
              sensorEl.innerText = 'ESTIMATING';
              sensorEl.style.color = 'var(--sub)';
            }

            if (d.arm === 0 && isArmed) {
              isArmed = false;
              updateArmUI();
            }
            if (d.alt === 0 && isAltHold) {
              isAltHold = false;
              leftStickRecenterY = false;
              updateAltUI();
            }

            if (d.arm || isArmed) {
              phoneFlightLog.push({
                t: performance.now(),
                r: d.r, p: d.p, y: d.y,
                h: d.h, v: d.v, lv: d.lv,
                arm: d.arm, alt: d.alt,
                m1: d.m ? d.m[0] : 0, m2: d.m ? d.m[1] : 0, m3: d.m ? d.m[2] : 0, m4: d.m ? d.m[3] : 0,
                thr: throttle, yaw: yaw, pitch: pitch, roll: roll
              });
              if (phoneFlightLog.length > 5000) phoneFlightLog.shift();
            }
            if (wasArmedPrev && !d.arm && phoneFlightLog.length > 10) {
              try { localStorage.setItem('drone_last_flight', JSON.stringify(phoneFlightLog)); } catch(e) {}
            }
            wasArmedPrev = d.arm;
          }
        } catch (e) {}
      };
      ws.onerror = () => { ws.close(); };
    } catch (e) { setTimeout(initWebSocket, 1000); }
  }
  initWebSocket();

  let trimRoll = parseInt(localStorage.getItem('drone_trim_r') || '0');
  let trimPitch = parseInt(localStorage.getItem('drone_trim_p') || '0');
  let trimYaw = parseInt(localStorage.getItem('drone_trim_y') || '0');

  function updateTrimUI() {
    document.getElementById('t-r').innerText = (trimRoll > 0 ? '+' : '') + trimRoll;
    document.getElementById('t-p').innerText = (trimPitch > 0 ? '+' : '') + trimPitch;
    document.getElementById('t-y').innerText = (trimYaw > 0 ? '+' : '') + trimYaw;
    localStorage.setItem('drone_trim_r', trimRoll);
    localStorage.setItem('drone_trim_p', trimPitch);
    localStorage.setItem('drone_trim_y', trimYaw);
  }
  function adjTrim(axis, delta) {
    if (axis === 'r') trimRoll = Math.max(-150, Math.min(150, trimRoll + delta));
    if (axis === 'p') trimPitch = Math.max(-150, Math.min(150, trimPitch + delta));
    if (axis === 'y') trimYaw = Math.max(-150, Math.min(150, trimYaw + delta));
    updateTrimUI();
  }
  function rstTrim() { trimRoll = 0; trimPitch = 0; trimYaw = 0; updateTrimUI(); }
  updateTrimUI();

  async function calibrateFlat() {
    const btn = document.getElementById('btn-cal');
    btn.innerText = 'CALIBRATING';
    try {
      await fetch('/calibrate');
      let checks = 0;
      const poll = setInterval(async () => {
        checks++;
        try {
          const res = await fetch('/cal_status');
          const data = await res.json();
          if (!data.active || checks >= 6) {
            clearInterval(poll);
            btn.innerText = 'CAL SAVED';
            setTimeout(() => btn.innerText = 'CAL', 3000);
          }
        } catch(e) {
          if (checks >= 6) { clearInterval(poll); btn.innerText = 'CAL'; }
        }
      }, 500);
    } catch(e) { btn.innerText = 'CAL'; }
  }

  function openPidModal() { document.getElementById('modal-pid').classList.add('open'); fetchPids(); }
  function closePidModal() { document.getElementById('modal-pid').classList.remove('open'); }

  function stepPid(id, delta) {
    const el = document.getElementById('inp-' + id);
    if (!el) return;
    el.value = Math.max(0, Math.min(250, (parseInt(el.value) || 0) + delta));
  }

  function fetchPids() {
    const st = document.getElementById('pid-status');
    st.innerText = 'FETCHING...';
    fetch('/get_pids')
      .then(r => r.json())
      .then(d => {
        ['rp','ri','rd','rf','pp','pi','pd','pf','yp','yi','yd','yf','lp','vp','vi'].forEach(k => {
          if (d[k] !== undefined && document.getElementById('inp-'+k)) document.getElementById('inp-'+k).value = d[k];
        });
        if (d.vs !== undefined && document.getElementById('inp-vs')) document.getElementById('inp-vs').value = d.vs;
        st.innerText = 'LOADED';
      })
      .catch(e => st.innerText = 'ERR');
  }

  function applyPidsLive() {
    const st = document.getElementById('pid-status');
    st.innerText = 'APPLYING...';
    const params = new URLSearchParams({});
    ['rp','ri','rd','rf','pp','pi','pd','pf','yp','yi','yd','yf','lp','vp','vi'].forEach(k => {
      if(document.getElementById('inp-'+k)) params.append(k, document.getElementById('inp-'+k).value);
    });
    if(document.getElementById('inp-vs')) params.append('vs', document.getElementById('inp-vs').value);
    
    fetch('/set_pids?' + params.toString())
      .then(r => r.json())
      .then(d => st.innerText = d.status === 'ok' ? 'APPLIED LIVE' : 'FAIL')
      .catch(e => st.innerText = 'ERR');
  }

  function savePidsEEPROM() {
    const st = document.getElementById('pid-status');
    st.innerText = 'SAVING...';
    fetch('/save_pids')
      .then(r => r.json())
      .then(d => st.innerText = d.status === 'saved' ? 'SAVED EEPROM' : 'FAIL')
      .catch(e => st.innerText = 'ERR');
  }

  let inFlight = false;
  let lastHttpSend = 0;
  setInterval(async () => {
    const now = performance.now();
    const armVal = isArmed ? 2000 : 1000;
    const altVal = isAltHold ? 2000 : 1000;
    const effRoll = Math.max(1000, Math.min(2000, roll + trimRoll));
    const effPitch = Math.max(1000, Math.min(2000, pitch + trimPitch));
    const effYaw = Math.max(1000, Math.min(2000, yaw + trimYaw));

    if (wsConnected && ws && ws.readyState === WebSocket.OPEN) {
      const buf = new Uint16Array(6);
      buf[0] = effRoll; buf[1] = effPitch; buf[2] = throttle;
      buf[3] = effYaw; buf[4] = armVal; buf[5] = altVal;
      ws.send(buf.buffer);
    } else {
      if (inFlight || (now - lastHttpSend < 30)) return;
      inFlight = true;
      lastHttpSend = now;
      const url = `/rc?r=${effRoll}&p=${effPitch}&t=${throttle}&y=${effYaw}&arm=${isArmed ? 1 : 0}&alt=${isAltHold ? 1 : 0}`;
      try { await fetch(url); } catch (e) {}
      inFlight = false;
    }
  }, 20);
</script>
</body>
</html>
)rawliteral";

const char FLIGHT_LOG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Drone Flight Recorder & Diagnostics</title>
<style>
  :root { --bg: #0b0f19; --card: #151d2f; --sky: #38bdf8; --green: #10b981; --red: #ef4444; --text: #f9fafb; --sub: #94a3b8; }
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, sans-serif; }
  body { background: var(--bg); color: var(--text); padding: 14px; }
  .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
  .btn { padding: 8px 14px; border-radius: 6px; background: #0284c7; color: #fff; text-decoration: none; font-weight: 700; border: none; cursor: pointer; font-size: 0.85rem; }
  .btn-danger { background: #dc2626; }
  .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 8px; margin-bottom: 14px; }
  .stat-card { background: var(--card); padding: 10px; border-radius: 8px; border: 1px solid #1e293b; }
  .stat-lbl { font-size: 0.68rem; color: var(--sub); font-weight: 700; text-transform: uppercase; }
  .stat-val { font-size: 1.1rem; color: #fff; font-weight: 800; font-family: monospace; margin-top: 2px; }
  .chart-box { background: var(--card); border: 1px solid #1e293b; border-radius: 8px; padding: 12px; margin-bottom: 14px; }
  table { width: 100%; border-collapse: collapse; font-size: 0.72rem; font-family: monospace; }
  th, td { padding: 5px 8px; border-bottom: 1px solid #1e293b; text-align: right; }
  th { background: #0f172a; color: var(--sub); text-align: right; }
</style>
</head>
<body>
  <div class="header">
    <h2>📊 ESP32 Flight Telemetry Log</h2>
    <div style="display:flex;gap:8px;">
      <a href="/" class="btn" style="background:#334155;">🕹️ Cockpit</a>
      <a href="/flightlog.csv" class="btn">📥 Download CSV</a>
      <button class="btn btn-danger" onclick="fetch('/clearlog').then(()=>location.reload())">🗑️ Clear Log</button>
    </div>
  </div>
  <div class="stats-grid" id="stats-area">
    <div class="stat-card"><div class="stat-lbl">TOTAL SAMPLES</div><div class="stat-val" id="st-samples">0</div></div>
    <div class="stat-card"><div class="stat-lbl">MAX ALTITUDE</div><div class="stat-val" id="st-maxalt" style="color:var(--sky);">0 cm</div></div>
    <div class="stat-card"><div class="stat-lbl">MAX CLIMB RATE</div><div class="stat-val" id="st-maxclimb" style="color:var(--green);">0 cm/s</div></div>
    <div class="stat-card"><div class="stat-lbl">LASER TRACKING</div><div class="stat-val" id="st-laser">100%</div></div>
  </div>
  <div class="chart-box">
    <h4 style="margin-bottom:8px;color:var(--sub);">ALTITUDE TRACE (LAST 800 SAMPLES)</h4>
    <canvas id="chart" width="900" height="250" style="width:100%;height:250px;background:#0b0f19;border-radius:6px;"></canvas>
  </div>
  <div class="chart-box" style="max-height:300px;overflow-y:auto;">
    <h4 style="margin-bottom:8px;color:var(--sub);">RECENT FLIGHT FRAMES</h4>
    <table id="log-table">
      <thead><tr><th>Time(ms)</th><th>Alt(cm)</th><th>Laser(cm)</th><th>Vario(cm/s)</th><th>Roll(°)</th><th>Pitch(°)</th><th>ThrOut(µs)</th><th>P-Term</th><th>I-Term</th><th>Status</th></tr></thead>
      <tbody></tbody>
    </table>
  </div>
<script>
  async function loadLog() {
    try {
      const res = await fetch('/flightlog.csv');
      const text = await res.text();
      const lines = text.trim().split('\n');
      if (lines.length <= 1) return;
      const rows = lines.slice(1).map(l => l.split(',').map(Number));
      
      document.getElementById('st-samples').innerText = rows.length;
      let maxAlt = 0, maxClimb = 0, laserValidCnt = 0;
      rows.forEach(r => {
        if (r[1] > maxAlt) maxAlt = r[1];
        if (r[4] > maxClimb) maxClimb = r[4];
        if (r[3] === 1) laserValidCnt++;
      });
      document.getElementById('st-maxalt').innerText = maxAlt + ' cm';
      document.getElementById('st-maxclimb').innerText = '+' + maxClimb + ' cm/s';
      document.getElementById('st-laser').innerText = Math.round((laserValidCnt / rows.length) * 100) + '%';

      // Canvas Chart
      const cvs = document.getElementById('chart');
      const ctx = cvs.getContext('2d');
      const W = cvs.width, H = cvs.height;
      ctx.clearRect(0, 0, W, H);
      
      ctx.strokeStyle = '#1e293b';
      ctx.lineWidth = 1;
      for(let y = 0; y < H; y += 40) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke(); }

      const maxVal = Math.max(120, maxAlt + 20);
      function getY(val) { return H - (val / maxVal) * (H - 30) - 15; }

      // Altitude Line
      ctx.strokeStyle = '#38bdf8';
      ctx.lineWidth = 2;
      ctx.beginPath();
      rows.forEach((r, idx) => {
        const x = (idx / (rows.length - 1)) * W;
        const y = getY(r[1]);
        if (idx === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      });
      ctx.stroke();

      // Laser Dots
      ctx.fillStyle = '#10b981';
      rows.forEach((r, idx) => {
        if (r[3] === 1) {
          const x = (idx / (rows.length - 1)) * W;
          const y = getY(r[2]);
          ctx.fillRect(x - 1, y - 1, 3, 3);
        }
      });

      // Populate Table (last 40 rows)
      const tbody = document.querySelector('#log-table tbody');
      tbody.innerHTML = '';
      rows.slice(-40).reverse().forEach(r => {
        const tr = document.createElement('tr');
        const stat = (r[12] ? 'ARMED ' : 'DISARM ') + (r[13] ? '[ALT]' : '');
        tr.innerHTML = `<td>${r[0]}</td><td style="color:#38bdf8">${r[1]}</td><td>${r[2]}</td><td>${r[4]}</td><td>${r[5]}</td><td>${r[6]}</td><td>${r[8]}</td><td>${r[9]}</td><td>${r[10]}</td><td style="color:#10b981">${stat}</td>`;
        tbody.appendChild(tr);
      });
    } catch(e) {}
  }
  loadLog();
</script>
</body>
</html>
)rawliteral";

#include <atomic>
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"

static String calcWebSocketAccept(const String& key)
{
  String fullKey = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char sha1Result[20];
  mbedtls_sha1_context ctx;
  mbedtls_sha1_init(&ctx);
  mbedtls_sha1_starts_ret(&ctx);
  mbedtls_sha1_update_ret(&ctx, (const unsigned char*)fullKey.c_str(), fullKey.length());
  mbedtls_sha1_finish_ret(&ctx, sha1Result);
  mbedtls_sha1_free(&ctx);

  unsigned char base64Result[36];
  size_t outLen = 0;
  mbedtls_base64_encode(base64Result, sizeof(base64Result), &outLen, sha1Result, 20);
  base64Result[outLen] = '\0';
  return String((char*)base64Result);
}

class InputWifi : public InputDevice
{
public:
  InputWifi(): _initialized(false), _model(nullptr), _lastPacketTime(0), _lastTelemetryTime(0), _lastLogSampleTime(0), _logHead(0), _logCount(0), _wsServer(81), _wsHandshakeDone(false)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      _channels[i].store((i == 2 || i == 4) ? 1000 : 1500, std::memory_order_relaxed);
    }
  }

  void setModel(Model* model)
  {
    _model = model;
  }

  int begin(const char* ssid = "ESP32-DRONE", const char* pass = "12345678", uint16_t udpPort = 8888)
  {
#ifdef ESP32
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector
#endif
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // Full Wi-Fi TX power for maximum link reliability
    WiFi.softAP(ssid, pass);
    _udp.begin(udpPort);

    // Setup HTTP Web Server on port 80
    _server.on("/", [this]() {
      _server.send_P(200, "text/html", JOYSTICK_PAGE);
    });

    _server.on("/log", [this]() {
      _server.send_P(200, "text/html", FLIGHT_LOG_PAGE);
    });

    _server.on("/clearlog", [this]() {
      _logHead = 0;
      _logCount = 0;
      _server.send(200, "text/plain", "LOG_CLEARED");
    });

    _server.on("/calibrate", [this]() {
      if (_model) {
        _model->calibrateGyro();
      }
      _server.send(200, "application/json", "{\"status\":\"calibrating\"}");
    });

    _server.on("/cal_status", [this]() {
      bool active = _model ? _model->calibrationActive() : false;
      _server.send(200, "application/json", active ? "{\"active\":true}" : "{\"active\":false}");
    });

    _server.on("/get_pids", [this]() {
      if (!_model) {
        _server.send(500, "application/json", "{}");
        return;
      }
      const auto& r = _model->config.pid[FC_PID_ROLL];
      const auto& p = _model->config.pid[FC_PID_PITCH];
      const auto& y = _model->config.pid[FC_PID_YAW];
      const auto& lvl = _model->config.pid[FC_PID_LEVEL];
      const auto& v = _model->config.pid[FC_PID_VEL];

      String json = "{";
      json += "\"rp\":" + String(r.P) + ",\"ri\":" + String(r.I) + ",\"rd\":" + String(r.D) + ",\"rf\":" + String(r.F) + ",";
      json += "\"pp\":" + String(p.P) + ",\"pi\":" + String(p.I) + ",\"pd\":" + String(p.D) + ",\"pf\":" + String(p.F) + ",";
      json += "\"yp\":" + String(y.P) + ",\"yi\":" + String(y.I) + ",\"yd\":" + String(y.D) + ",\"yf\":" + String(y.F) + ",";
      json += "\"lp\":" + String(lvl.P) + ",";
      json += "\"vp\":" + String(v.P) + ",\"vi\":" + String(v.I) + ",\"vf\":" + String(v.F) + ",";
      json += "\"vs\":" + String(_model->config.vbat.scale);
      json += "}";
      _server.send(200, "application/json", json);
    });

    _server.on("/set_pids", [this]() {
      if (!_model) {
        _server.send(500, "application/json", "{\"status\":\"error\"}");
        return;
      }
      if (_server.hasArg("rp")) _model->config.pid[FC_PID_ROLL].P = _server.arg("rp").toInt();
      if (_server.hasArg("ri")) _model->config.pid[FC_PID_ROLL].I = _server.arg("ri").toInt();
      if (_server.hasArg("rd")) _model->config.pid[FC_PID_ROLL].D = _server.arg("rd").toInt();
      if (_server.hasArg("rf")) _model->config.pid[FC_PID_ROLL].F = _server.arg("rf").toInt();

      if (_server.hasArg("pp")) _model->config.pid[FC_PID_PITCH].P = _server.arg("pp").toInt();
      if (_server.hasArg("pi")) _model->config.pid[FC_PID_PITCH].I = _server.arg("pi").toInt();
      if (_server.hasArg("pd")) _model->config.pid[FC_PID_PITCH].D = _server.arg("pd").toInt();
      if (_server.hasArg("pf")) _model->config.pid[FC_PID_PITCH].F = _server.arg("pf").toInt();

      if (_server.hasArg("yp")) _model->config.pid[FC_PID_YAW].P = _server.arg("yp").toInt();
      if (_server.hasArg("yi")) _model->config.pid[FC_PID_YAW].I = _server.arg("yi").toInt();
      if (_server.hasArg("yd")) _model->config.pid[FC_PID_YAW].D = _server.arg("yd").toInt();
      if (_server.hasArg("yf")) _model->config.pid[FC_PID_YAW].F = _server.arg("yf").toInt();

      if (_server.hasArg("lp")) _model->config.pid[FC_PID_LEVEL].P = _server.arg("lp").toInt();

      if (_server.hasArg("vp")) _model->config.pid[FC_PID_VEL].P = _server.arg("vp").toInt();
      if (_server.hasArg("vi")) _model->config.pid[FC_PID_VEL].I = _server.arg("vi").toInt();
      if (_server.hasArg("vf")) _model->config.pid[FC_PID_VEL].F = _server.arg("vf").toInt();

      if (_server.hasArg("vs")) _model->config.vbat.scale = std::clamp((int)_server.arg("vs").toInt(), 50, 250);

      _model->reloadPids();
      _server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    _server.on("/save_pids", [this]() {
      if (!_model) {
        _server.send(500, "application/json", "{\"status\":\"error\"}");
        return;
      }
      _model->save();
      _server.send(200, "application/json", "{\"status\":\"saved\"}");
    });

    _server.on("/flightlog.csv", [this]() {
      String csv = "time_ms,height_cm,laser_cm,laser_valid,vario_cms,roll_deg,pitch_deg,stick_thr,out_thr,p_term,i_term,d_term,armed,althold\r\n";
      size_t start = (_logCount < 800) ? 0 : _logHead;
      for (size_t i = 0; i < _logCount; ++i)
      {
        size_t idx = (start + i) % 800;
        const auto& e = _flightLog[idx];
        csv += String(e.timeMs) + "," +
               String(e.heightCm) + "," +
               String(e.laserCm) + "," +
               String((e.flags & 4) ? 1 : 0) + "," +
               String(e.varioCms) + "," +
               String(e.rollDeg / 10.0f, 1) + "," +
               String(e.pitchDeg / 10.0f, 1) + "," +
               String(e.stickThr) + "," +
               String(e.outThr) + "," +
               String(e.pTerm) + "," +
               String(e.iTerm) + "," +
               String(e.dTerm) + "," +
               String((e.flags & 1) ? 1 : 0) + "," +
               String((e.flags & 2) ? 1 : 0) + "\r\n";
      }
      _server.send(200, "text/csv", csv);
    });

    _server.on("/rc", [this]() {
      int args = _server.args();
      for (int i = 0; i < args; ++i)
      {
        const String& name = _server.argName(i);
        int val = _server.arg(i).toInt();
        if (name.length() == 1)
        {
          switch (name[0])
          {
            case 'r': _channels[0].store(std::clamp(val, 1000, 2000), std::memory_order_release); break;
            case 'p': _channels[1].store(std::clamp(val, 1000, 2000), std::memory_order_release); break;
            case 't': _channels[2].store(std::clamp(val, 1000, 2000), std::memory_order_release); break;
            case 'y': _channels[3].store(std::clamp(val, 1000, 2000), std::memory_order_release); break;
          }
        }
        else if (name.equals("arm")) _channels[4].store(val ? 2000 : 1000, std::memory_order_release);
        else if (name.equals("alt")) _channels[5].store(val ? 2000 : 1000, std::memory_order_release);
      }
      _lastPacketTime.store(millis(), std::memory_order_release);
      _server.send(200, "text/plain", "OK");
    });

    _server.begin();

    // Start WebSocket Server on port 81 (TCP_NODELAY for sub-2ms latency)
    _wsServer.begin();
    _wsServer.setNoDelay(true);
    _wsHandshakeDone = false;
    _wsReqBuf = "";

    _initialized = true;
    _lastPacketTime.store(millis(), std::memory_order_release);
    return 1;
  }

  // Network handler executed on Core 0
  void handleNetwork()
  {
    if (!_initialized) return;
    _server.handleClient();

    // 1. WebSocket Handler on Port 81
    if (!_wsClient || !_wsClient.connected())
    {
      WiFiClient newClient = _wsServer.available();
      if (newClient)
      {
        _wsClient = newClient;
        _wsClient.setNoDelay(true);
        _wsClient.setTimeout(5); // 5ms maximum read timeout to prevent Core 0 stalls
        _wsHandshakeDone = false;
        _wsReqBuf = "";
      }
    }

    if (_wsClient && _wsClient.connected())
    {
      if (!_wsHandshakeDone)
      {
        while (_wsClient.available())
        {
          char c = _wsClient.read();
          _wsReqBuf += c;
          if (_wsReqBuf.endsWith("\r\n\r\n"))
          {
            int keyIdx = _wsReqBuf.indexOf("Sec-WebSocket-Key: ");
            if (keyIdx >= 0)
            {
              int keyEnd = _wsReqBuf.indexOf("\r\n", keyIdx);
              String key = _wsReqBuf.substring(keyIdx + 19, keyEnd);
              key.trim();
              String acceptKey = calcWebSocketAccept(key);
              String response = "HTTP/1.1 101 Switching Protocols\r\n"
                                "Upgrade: websocket\r\n"
                                "Connection: Upgrade\r\n"
                                "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
              _wsClient.print(response);
              _wsHandshakeDone = true;
              _wsReqBuf = "";
            }
            else
            {
              _wsClient.stop();
              _wsHandshakeDone = false;
              _wsReqBuf = "";
            }
            break;
          }
          if (_wsReqBuf.length() > 1024)
          {
            _wsClient.stop();
            _wsHandshakeDone = false;
            _wsReqBuf = "";
            break;
          }
        }
      }
      else
      {
        // 1. Process Incoming Binary Control Frames (50Hz from phone)
        while (_wsClient.available() >= 18)
        {
          uint8_t b0 = _wsClient.read();
          uint8_t b1 = _wsClient.read();
          uint8_t opcode = b0 & 0x0F;

          if (opcode == 0x08) // Close frame
          {
            _wsClient.stop();
            _wsHandshakeDone = false;
            break;
          }
          if (opcode == 0x09) // Ping frame: respond with Pong
          {
            uint8_t pong[2] = {0x8A, 0x00};
            _wsClient.write(pong, 2);
            continue;
          }
          if (opcode == 0x0A) // Pong frame from browser: safely ignore
          {
            uint8_t pongLen = b1 & 0x7F;
            int discard = (b1 & 0x80 ? 4 : 0) + pongLen;
            while (discard-- > 0 && _wsClient.available()) _wsClient.read();
            continue;
          }

          uint8_t len = b1 & 0x7F;
          bool masked = (b1 & 0x80) != 0;

          if (opcode != 0x02 || !masked || len != 12)
          {
            // Discard unhandled/malformed frame instead of terminating the link
            int discard = (masked ? 4 : 0) + len;
            while (discard-- > 0 && _wsClient.available()) _wsClient.read();
            continue;
          }

          uint8_t frameBuf[16];
          int bytesRead = _wsClient.readBytes((char*)frameBuf, 16);
          if (bytesRead != 16)
          {
            continue;
          }

          uint8_t* mask = frameBuf;
          uint8_t* payload = frameBuf + 4;
          for (int i = 0; i < 12; ++i)
          {
            payload[i] ^= mask[i % 4];
          }

          uint16_t* u16 = (uint16_t*)payload;

          bool rpytValid = (u16[0] >= 900 && u16[0] <= 2100) &&
                           (u16[1] >= 900 && u16[1] <= 2100) &&
                           (u16[2] >= 900 && u16[2] <= 2100) &&
                           (u16[3] >= 900 && u16[3] <= 2100);
          bool armValid = (u16[4] == 1000 || u16[4] == 2000);
          bool altValid = (u16[5] == 1000 || u16[5] == 2000);

          if (rpytValid && armValid && altValid)
          {
            _channels[0].store(u16[0], std::memory_order_release);
            _channels[1].store(u16[1], std::memory_order_release);
            _channels[2].store(u16[2], std::memory_order_release);
            _channels[3].store(u16[3], std::memory_order_release);
            _channels[4].store(u16[4], std::memory_order_release);
            _channels[5].store(u16[5], std::memory_order_release);
            _lastPacketTime.store(millis(), std::memory_order_release);
          }
        }

        // 2. Stream Live Telemetry HUD back to Phone (20Hz)
        uint32_t now = millis();
        if (_model && (now - _lastTelemetryTime >= 50))
        {
          _lastTelemetryTime = now;
          
          float r = Utils::toDeg(_model->state.attitude.euler[AXIS_ROLL]);
          float p = Utils::toDeg(_model->state.attitude.euler[AXIS_PITCH]);
          float y = Utils::toDeg(_model->state.attitude.euler[AXIS_YAW]);
          int h = (int)lrintf(_model->state.altitude.height * 100.0f);
          int v = (int)lrintf(_model->state.altitude.vario * 100.0f);
          bool lv = _model->state.rangefinder.valid;
          bool arm = _model->isModeActive(MODE_ARMED);
          bool ang = _model->isModeActive(MODE_ANGLE);
          bool alt = _model->isModeActive(MODE_ALTHOLD);
          int m1 = std::clamp((int)lrintf(_model->state.output.ch[0] * 100.0f), 0, 100);
          int m2 = std::clamp((int)lrintf(_model->state.output.ch[1] * 100.0f), 0, 100);
          int m3 = std::clamp((int)lrintf(_model->state.output.ch[2] * 100.0f), 0, 100);
          int m4 = std::clamp((int)lrintf(_model->state.output.ch[3] * 100.0f), 0, 100);
          float vb = _model->state.battery.voltage;
          int vp = (int)lrintf(_model->state.battery.percentage);

          char tbuf[192];
          int tlen = snprintf(tbuf, sizeof(tbuf),
                              "{\"r\":%.1f,\"p\":%.1f,\"y\":%.1f,\"h\":%d,\"v\":%d,\"lv\":%d,\"arm\":%d,\"ang\":%d,\"alt\":%d,\"m\":[%d,%d,%d,%d],\"vb\":%.2f,\"vp\":%d}",
                              r, p, y, h, v, lv ? 1 : 0, arm ? 1 : 0, ang ? 1 : 0, alt ? 1 : 0, m1, m2, m3, m4, vb, vp);

          if (tlen > 0 && tlen <= 125)
          {
            uint8_t hdr[2] = { 0x81, (uint8_t)tlen };
            _wsClient.write(hdr, 2);
            _wsClient.write((const uint8_t*)tbuf, tlen);
          }
          else if (tlen > 125 && tlen <= 65535)
          {
            uint8_t hdr[4] = { 0x81, 126, (uint8_t)(tlen >> 8), (uint8_t)(tlen & 0xFF) };
            _wsClient.write(hdr, 4);
            _wsClient.write((const uint8_t*)tbuf, tlen);
          }
        }
      }
    }

    // 2. Sample In-Memory Flight Diagnostics Ring Buffer (40Hz / 25ms)
    uint32_t now = millis();
    if (_model && (now - _lastLogSampleTime >= 25))
    {
      _lastLogSampleTime = now;
      FlightLogEntry& entry = _flightLog[_logHead];
      entry.timeMs = now;
      entry.heightCm = (int16_t)lrintf(_model->state.altitude.height * 100.0f);
      entry.targetCm = (int16_t)lrintf(_model->state.debug[2]);
      entry.laserCm = (int16_t)lrintf(_model->state.rangefinder.distance * 100.0f);
      entry.varioCms = (int16_t)lrintf(_model->state.altitude.vario * 100.0f);
      entry.rollDeg = (int16_t)lrintf(Utils::toDeg(_model->state.attitude.euler[AXIS_ROLL]) * 10.0f);
      entry.pitchDeg = (int16_t)lrintf(Utils::toDeg(_model->state.attitude.euler[AXIS_PITCH]) * 10.0f);
      entry.stickThr = (int16_t)_channels[2].load(std::memory_order_relaxed);
      entry.outThr = (int16_t)lrintf((_model->state.output.ch[AXIS_THRUST] + 1.0f) * 500.0f + 1000.0f);
      entry.pTerm = (int16_t)_model->state.debug[4];
      entry.iTerm = (int16_t)_model->state.debug[5];
      entry.dTerm = (int16_t)_model->state.debug[6];
      
      uint8_t flags = 0;
      if (_model->isModeActive(MODE_ARMED)) flags |= 1;
      if (_model->isModeActive(MODE_ALTHOLD)) flags |= 2;
      if (_model->state.rangefinder.valid) flags |= 4;
      entry.flags = flags;

      _logHead = (_logHead + 1) % 800;
      if (_logCount < 800) _logCount++;
    }

    // 3. Check UDP Packets (port 8888)
    int packetSize = _udp.parsePacket();
    if (packetSize > 0)
    {
      uint8_t buf[64];
      int len = _udp.read(buf, sizeof(buf));
      if (len >= 16)
      {
        uint16_t* u16 = (uint16_t*)buf;
        bool rpytValid = (u16[0] >= 800 && u16[0] <= 2200) &&
                         (u16[1] >= 800 && u16[1] <= 2200) &&
                         (u16[2] >= 800 && u16[2] <= 2200) &&
                         (u16[3] >= 800 && u16[3] <= 2200);
        if (rpytValid)
        {
          for(size_t i = 0; i < 8; ++i)
          {
            if (u16[i] >= 800 && u16[i] <= 2200) _channels[i].store(u16[i], std::memory_order_release);
          }
          _lastPacketTime.store(millis(), std::memory_order_release);
        }
      }
    }
  }

  // Real-time Flight loop update (Core 1) - instantaneous, non-blocking
  InputStatus update() override
  {
    if (!_initialized) return INPUT_LOST;

    uint32_t now = millis();
    // 1.5s Failsafe Timeout
    if (now - _lastPacketTime.load(std::memory_order_acquire) > 1500)
    {
      _channels[2].store(1000, std::memory_order_relaxed); // Min Throttle
      _channels[4].store(1000, std::memory_order_relaxed); // Disarm
      return INPUT_FAILSAFE;
    }

    return INPUT_RECEIVED;
  }

  uint16_t get(uint8_t channel) const override
  {
    if (channel < 8) return _channels[channel].load(std::memory_order_acquire);
    return 1500;
  }

  void get(uint16_t* data, size_t len) const override
  {
    for(size_t i = 0; i < len; ++i)
    {
      data[i] = get(i);
    }
  }

  size_t getChannelCount() const override { return 8; }
  bool needAverage() const override { return true; }

private:
  bool _initialized;
  Model* _model;
  std::atomic<uint16_t> _channels[8];
  std::atomic<uint32_t> _lastPacketTime;
  uint32_t _lastTelemetryTime;
  uint32_t _lastLogSampleTime;
  
  // In-Memory Flight Diagnostics Ring Buffer
  FlightLogEntry _flightLog[800];
  size_t _logHead;
  size_t _logCount;

  WiFiUDP _udp;
  WebServer _server;
  WiFiServer _wsServer;
  WiFiClient _wsClient;
  bool _wsHandshakeDone;
  String _wsReqBuf;
};

} // namespace Espfc::Device


