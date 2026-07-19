#pragma once

const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
    <title>ESP-FC Flight Deck</title>
    <style>
        :root {
            --bg-color: #000000;
            --panel-bg: rgba(10, 10, 12, 0.45);
            --panel-border: rgba(255, 255, 255, 0.08);
            --text-primary: #ffffff;
            --text-secondary: #86868b;
            --accent-blue: #2997ff;
            --accent-red: #ff3b30;
            --accent-green: #30d158;
            --glow-blue: rgba(41, 151, 255, 0.15);
            --glow-red: rgba(255, 59, 48, 0.25);
            --font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            --transition-apple: all 0.5s cubic-bezier(0.16, 1, 0.3, 1);
        }

        * {
            box-sizing: border-box;
            user-select: none;
            -webkit-user-select: none;
            margin: 0;
            padding: 0;
            -webkit-tap-highlight-color: transparent;
        }

        body {
            background-color: var(--bg-color);
            color: var(--text-primary);
            font-family: var(--font-family);
            overflow: hidden;
            width: 100vw;
            height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: space-between;
            padding: 20px;
            background-image: 
                radial-gradient(circle at 15% 15%, rgba(41, 151, 255, 0.08) 0%, transparent 40%),
                radial-gradient(circle at 85% 85%, rgba(189, 0, 255, 0.05) 0%, transparent 45%);
            letter-spacing: -0.01em;
        }

        /* Ambient Glow Backdrop */
        .ambient-glow {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            width: 600px;
            height: 600px;
            background: radial-gradient(circle, rgba(41, 151, 255, 0.03) 0%, transparent 70%);
            pointer-events: none;
            z-index: 0;
            transition: var(--transition-apple);
        }

        body.armed-state .ambient-glow {
            background: radial-gradient(circle, rgba(255, 59, 48, 0.05) 0%, transparent 70%);
        }

        header {
            width: 100%;
            max-width: 1200px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 12px 20px;
            background: var(--panel-bg);
            backdrop-filter: blur(20px);
            -webkit-backdrop-filter: blur(20px);
            border: 1px solid var(--panel-border);
            border-radius: 16px;
            z-index: 10;
            box-shadow: 0 4px 30px rgba(0, 0, 0, 0.4);
        }

        h1 {
            font-size: 1rem;
            font-weight: 600;
            letter-spacing: 0.08em;
            text-transform: uppercase;
            color: var(--text-primary);
            opacity: 0.9;
        }

        .status-badge {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 6px 14px;
            background: rgba(255, 255, 255, 0.04);
            border: 1px solid rgba(255, 255, 255, 0.06);
            border-radius: 99px;
            transition: var(--transition-apple);
        }

        .btn-fullscreen {
            background: rgba(255, 255, 255, 0.04);
            border: 1px solid rgba(255, 255, 255, 0.06);
            color: var(--text-secondary);
            border-radius: 50%;
            width: 34px;
            height: 34px;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            transition: var(--transition-apple);
        }

        .btn-fullscreen:hover {
            background: rgba(255, 255, 255, 0.1);
            color: var(--text-primary);
            transform: scale(1.05);
        }

        .btn-fullscreen:active {
            transform: scale(0.95);
        }

        .status-dot {
            width: 6px;
            height: 6px;
            border-radius: 50%;
            background-color: var(--accent-red);
            box-shadow: 0 0 8px var(--accent-red);
            transition: var(--transition-apple);
        }

        .status-dot.connected {
            background-color: var(--accent-green);
            box-shadow: 0 0 8px var(--accent-green);
        }

        .status-text {
            font-size: 0.7rem;
            color: var(--text-secondary);
            font-weight: 500;
            text-transform: uppercase;
            letter-spacing: 0.05em;
        }

        .main-layout {
            width: 100%;
            max-width: 1200px;
            flex: 1;
            display: flex;
            justify-content: space-between;
            align-items: center;
            position: relative;
            margin: 16px 0;
            z-index: 5;
        }

        /* Joystick Glass Cylinder Design */
        .joystick-outer {
            width: 250px;
            height: 250px;
            background: radial-gradient(circle at 50% 50%, rgba(255, 255, 255, 0.02) 0%, rgba(255, 255, 255, 0.005) 100%);
            border: 1px solid var(--panel-border);
            border-radius: 50%;
            position: relative;
            display: flex;
            align-items: center;
            justify-content: center;
            box-shadow: 
                0 20px 40px rgba(0, 0, 0, 0.6),
                inset 0 1px 0 rgba(255, 255, 255, 0.05);
        }

        /* Crosshairs - Super Minimal Apple Style */
        .crosshair-h, .crosshair-v {
            position: absolute;
            background: rgba(255, 255, 255, 0.05);
            pointer-events: none;
        }
        .crosshair-h {
            left: 20px; right: 20px; height: 1px; top: 50%;
        }
        .crosshair-v {
            top: 20px; bottom: 20px; width: 1px; left: 50%;
        }

        /* 3D Machined Metal Joystick Knob */
        .joystick-knob {
            width: 68px;
            height: 68px;
            border-radius: 50%;
            background: radial-gradient(circle at 35% 35%, #2c2c2e 0%, #1c1c1e 100%);
            position: absolute;
            cursor: pointer;
            box-shadow: 
                0 12px 24px rgba(0, 0, 0, 0.6),
                inset 0 1.5px 2px rgba(255, 255, 255, 0.15),
                inset 0 -1.5px 2px rgba(0, 0, 0, 0.4);
            display: flex;
            align-items: center;
            justify-content: center;
            touch-action: none;
        }

        /* Inner Ring & Center Dot */
        .joystick-knob::before {
            content: '';
            width: 38px;
            height: 38px;
            border-radius: 50%;
            background: radial-gradient(circle at 50% 50%, #252525 0%, #151515 100%);
            border: 1px solid rgba(0, 0, 0, 0.4);
            box-shadow: inset 0 1px 2px rgba(255, 255, 255, 0.05);
        }

        .joystick-knob::after {
            content: '';
            position: absolute;
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--text-secondary);
            transition: var(--transition-apple);
        }

        #left-knob {
            border: 1px solid rgba(41, 151, 255, 0.3);
        }
        #left-knob::after {
            background: var(--accent-blue);
            box-shadow: 0 0 10px var(--accent-blue);
        }

        #right-knob {
            border: 1px solid rgba(189, 0, 255, 0.3);
        }
        #right-knob::after {
            background: #bd00ff;
            box-shadow: 0 0 10px #bd00ff;
        }

        /* Center Control Panel */
        .center-panel {
            flex: 1;
            max-width: 380px;
            display: flex;
            flex-direction: column;
            gap: 16px;
            align-items: center;
            justify-content: center;
            padding: 0 16px;
        }

        /* Tactile Slide-to-Arm Slider */
        .slider-arm-container {
            width: 100%;
            height: 54px;
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid var(--panel-border);
            border-radius: 27px;
            position: relative;
            display: flex;
            align-items: center;
            justify-content: center;
            overflow: hidden;
            box-shadow: inset 0 2px 4px rgba(0,0,0,0.5);
            backdrop-filter: blur(10px);
            -webkit-backdrop-filter: blur(10px);
        }

        .slider-arm-text {
            font-size: 0.75rem;
            font-weight: 600;
            color: var(--text-secondary);
            letter-spacing: 0.1em;
            text-transform: uppercase;
            pointer-events: none;
            z-index: 1;
            transition: var(--transition-apple);
            padding-left: 24px; /* Default center-ish layout */
        }

        .slider-arm-handle {
            position: absolute;
            left: 4px;
            width: 46px;
            height: 46px;
            border-radius: 23px;
            background: linear-gradient(135deg, #ffffff 0%, #d1d1d6 100%);
            box-shadow: 0 3px 6px rgba(0, 0, 0, 0.3);
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 2;
            transition: background 0.3s;
            touch-action: none;
        }

        .slider-arm-handle::after {
            content: '➔';
            font-size: 0.9rem;
            color: #1c1c1e;
            transition: var(--transition-apple);
        }

        /* Arm States */
        .slider-arm-container.armed-bg {
            background: rgba(255, 59, 48, 0.1);
            border-color: rgba(255, 59, 48, 0.3);
        }

        .slider-arm-container.armed-bg .slider-arm-handle {
            background: linear-gradient(135deg, var(--accent-red) 0%, #ff453a 100%);
            box-shadow: 0 0 15px var(--glow-red);
        }

        .slider-arm-container.armed-bg .slider-arm-handle::after {
            content: '✕';
            color: #ffffff;
            transform: rotate(90deg);
        }

        .slider-arm-container.armed-bg .slider-arm-text {
            color: var(--accent-red);
            text-shadow: 0 0 8px var(--glow-red);
            padding-left: 0;
            padding-right: 48px; /* Push text left to avoid overlapping handle on the right */
        }

        /* Premium Altitude Hold Button */
        .btn-althold {
            width: 100%;
            height: 48px;
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid var(--panel-border);
            border-radius: 24px;
            color: var(--text-secondary);
            font-size: 0.75rem;
            font-weight: 600;
            letter-spacing: 0.1em;
            text-transform: uppercase;
            cursor: pointer;
            transition: var(--transition-apple);
            box-shadow: inset 0 1px 2px rgba(255,255,255,0.05);
            backdrop-filter: blur(10px);
            -webkit-backdrop-filter: blur(10px);
            margin-top: 12px;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .btn-althold.active {
            background: rgba(48, 209, 88, 0.1);
            border-color: rgba(48, 209, 88, 0.3);
            color: #30d158;
            text-shadow: 0 0 8px rgba(48, 209, 88, 0.4);
            box-shadow: 0 0 10px rgba(48, 209, 88, 0.1);
        }

        /* Telemetry Dashboard Card */
        .telemetry-card {
            width: 100%;
            background: var(--panel-bg);
            backdrop-filter: blur(20px);
            -webkit-backdrop-filter: blur(20px);
            border: 1px solid var(--panel-border);
            border-radius: 20px;
            padding: 16px 20px;
            box-shadow: 0 15px 30px rgba(0, 0, 0, 0.5);
            display: flex;
            flex-direction: column;
            gap: 12px;
        }

        .grid-4 {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 12px;
        }

        .metric-box {
            display: flex;
            flex-direction: column;
            gap: 4px;
        }

        .metric-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .metric-label {
            font-size: 0.65rem;
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 0.08em;
            font-weight: 600;
        }

        .metric-value {
            font-size: 1.1rem;
            font-weight: 700;
            font-family: -apple-system-monospace, "SF Mono", Courier, monospace;
            color: var(--text-primary);
        }

        /* Horizontal progress bar for channels */
        .metric-bar-bg {
            width: 100%;
            height: 3px;
            background: rgba(255, 255, 255, 0.05);
            border-radius: 2px;
            overflow: hidden;
        }

        .metric-bar-fill {
            height: 100%;
            width: 50%; /* Default center */
            background: var(--text-secondary);
            border-radius: 2px;
            transition: width 0.1s ease-out, background-color 0.3s;
        }

        #bar-thr { background: var(--accent-blue); }
        #bar-yaw { background: var(--accent-blue); }
        #bar-pit { background: #bd00ff; }
        #bar-rol { background: #bd00ff; }

        /* Label positions around joysticks */
        .lbl-container {
            position: absolute;
            font-size: 0.6rem;
            color: var(--text-secondary);
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.1em;
            pointer-events: none;
            transition: var(--transition-apple);
        }

        .lbl-top { top: -20px; left: 50%; transform: translateX(-50%); }
        .lbl-bottom { bottom: -20px; left: 50%; transform: translateX(-50%); }
        .lbl-left { left: -26px; top: 50%; transform: translateY(-50%) rotate(-90deg); }
        .lbl-right { right: -26px; top: 50%; transform: translateY(-50%) rotate(90deg); }

        /* Fullscreen Ripple / Lens Flare Animation - GPU Hardware Accelerated */
        .ripple-wave {
            position: fixed;
            border-radius: 50%;
            pointer-events: none;
            z-index: 9999;
            background: transparent;
            border: 2px solid rgba(41, 151, 255, 0.4);
            box-shadow: 0 0 20px rgba(41, 151, 255, 0.2);
            transform: translate(-50%, -50%);
            animation: ripple-expansion 0.8s cubic-bezier(0.1, 0.8, 0.1, 1) forwards;
            will-change: width, height, opacity;
        }

        @keyframes ripple-expansion {
            0% {
                width: 0;
                height: 0;
                opacity: 1;
            }
            100% {
                width: 300vmax;
                height: 300vmax;
                opacity: 0;
            }
        }

        /* Screen Scale Entrance Animation - GPU Hardware Accelerated */
        .apple-transition-active {
            animation: apple-entrance 0.5s cubic-bezier(0.16, 1, 0.3, 1) forwards;
            will-change: transform, opacity;
        }

        @keyframes apple-entrance {
            0% {
                transform: scale(0.98);
                opacity: 0.9;
            }
            100% {
                transform: scale(1);
                opacity: 1;
            }
        }

        /* ========================================== */
        /* LANDSCAPE MODE HEIGHT FIXES (CRITICAL)     */
        /* ========================================== */
        @media (max-height: 480px) {
            body {
                padding: 8px 16px;
            }
            header {
                padding: 6px 16px;
                border-radius: 12px;
            }
            .main-layout {
                margin: 8px 0;
            }
            .joystick-outer {
                width: 160px;
                height: 160px;
            }
            .joystick-knob {
                width: 48px;
                height: 48px;
            }
            .joystick-knob::before {
                width: 24px;
                height: 24px;
            }
            .center-panel {
                max-width: 280px;
                gap: 8px;
            }
            .telemetry-card {
                padding: 10px 14px;
                border-radius: 14px;
                gap: 6px;
            }
            .metric-value {
                font-size: 0.95rem;
            }
            .slider-arm-container {
                height: 40px;
                border-radius: 20px;
            }
            .btn-althold {
                height: 36px;
                border-radius: 18px;
                font-size: 0.65rem;
                margin-top: 8px;
            }
            .slider-arm-handle {
                width: 32px;
                height: 32px;
                border-radius: 16px;
            }
            .slider-arm-text {
                font-size: 0.65rem;
            }
            .slider-arm-container.armed-bg .slider-arm-text {
                padding-right: 32px;
            }
            .lbl-container {
                font-size: 0.5rem;
            }
            .lbl-top { top: -14px; }
            .lbl-bottom { bottom: -14px; }
            .lbl-left { left: -22px; }
            .lbl-right { right: -22px; }
        }

        /* Portrait layout fallback scaling */
        @media (max-width: 500px) and (min-height: 480px) {
            .main-layout {
                flex-direction: row;
                justify-content: space-between;
                align-items: flex-end;
            }
            .joystick-outer {
                width: 150px;
                height: 150px;
            }
            .joystick-knob {
                width: 46px;
                height: 46px;
            }
            .center-panel {
                position: absolute;
                top: 0;
                left: 50%;
                transform: translateX(-50%);
                width: 100%;
                max-width: 320px;
            }
        }
    </style>
</head>
<body>
    <div class="ambient-glow"></div>

    <header>
        <h1>Flight Deck</h1>
        <div style="display: flex; align-items: center; gap: 12px;">
            <button id="btn-fullscreen" class="btn-fullscreen" title="Toggle Fullscreen">
                <svg viewBox="0 0 24 24" width="16" height="16" stroke="currentColor" stroke-width="2.1" fill="none" stroke-linecap="round" stroke-linejoin="round"><path d="M8 3H5a2 2 0 0 0-2 2v3m18 0V5a2 2 0 0 0-2-2h-3m0 18h3a2 2 0 0 0 2-2v-3M3 16v3a2 2 0 0 0 2 2h3"></path></svg>
            </button>
            <div class="status-badge">
                <div id="status-dot" class="status-dot"></div>
                <span id="status-text" class="status-text">Offline</span>
            </div>
        </div>
    </header>

    <div class="main-layout">
        <!-- Left Joystick (Throttle / Yaw) -->
        <div class="joystick-outer" id="left-container">
            <div class="crosshair-h"></div>
            <div class="crosshair-v"></div>
            <span class="lbl-container lbl-top">Throttle</span>
            <span class="lbl-container lbl-bottom">Cut</span>
            <span class="lbl-container lbl-left">Yaw L</span>
            <span class="lbl-container lbl-right">Yaw R</span>
            <div class="joystick-knob" id="left-knob"></div>
        </div>

        <!-- Center Control Panel -->
        <div class="center-panel">
            <!-- Slide-to-Arm safety switch -->
            <div class="slider-arm-container" id="slider-arm-container">
                <div class="slider-arm-handle" id="slider-arm-handle"></div>
                <span class="slider-arm-text" id="slider-arm-text">Slide to Arm</span>
            </div>

            <!-- Altitude Hold toggle button -->
            <button id="btn-althold" class="btn-althold">Altitude Hold: OFF</button>

            <!-- Telemetry Dashboard -->
            <div class="telemetry-card">
                <div class="grid-4">
                    <div class="metric-box">
                        <div class="metric-header">
                            <span class="metric-label">Thr</span>
                            <span class="metric-value" id="val-thr">1000</span>
                        </div>
                        <div class="metric-bar-bg">
                            <div class="metric-bar-fill" id="bar-thr" style="width: 0%;"></div>
                        </div>
                    </div>
                    <div class="metric-box">
                        <div class="metric-header">
                            <span class="metric-label">Yaw</span>
                            <span class="metric-value" id="val-yaw">1500</span>
                        </div>
                        <div class="metric-bar-bg">
                            <div class="metric-bar-fill" id="bar-yaw" style="width: 50%;"></div>
                        </div>
                    </div>
                    <div class="metric-box">
                        <div class="metric-header">
                            <span class="metric-label">Pitch</span>
                            <span class="metric-value" id="val-pit">1500</span>
                        </div>
                        <div class="metric-bar-bg">
                            <div class="metric-bar-fill" id="bar-pit" style="width: 50%;"></div>
                        </div>
                    </div>
                    <div class="metric-box">
                        <div class="metric-header">
                            <span class="metric-label">Roll</span>
                            <span class="metric-value" id="val-rol">1500</span>
                        </div>
                        <div class="metric-bar-bg">
                            <div class="metric-bar-fill" id="bar-rol" style="width: 50%;"></div>
                        </div>
                    </div>
                </div>
                <!-- Battery Telemetry Row -->
                <div style="display: flex; justify-content: space-between; border-top: 1px solid var(--panel-border); padding-top: 12px; margin-top: 4px; gap: 16px;">
                    <div style="display: flex; align-items: center; gap: 8px; flex: 1;">
                        <svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="color: var(--text-secondary);" id="battery-icon">
                            <rect x="1" y="6" width="18" height="12" rx="2" ry="2"></rect>
                            <line x1="23" y1="11" x2="23" y2="13"></line>
                        </svg>
                        <div style="flex: 1; display: flex; flex-direction: column; gap: 2px;">
                            <div style="display: flex; justify-content: space-between; align-items: center;">
                                <span class="metric-label" style="font-size: 0.75rem;">Battery</span>
                                <span class="metric-value" style="font-size: 0.85rem;" id="val-bat-pct">0%</span>
                            </div>
                            <div class="metric-bar-bg" style="height: 6px;">
                                <div class="metric-bar-fill" id="bar-bat" style="width: 0%; background-color: var(--accent-green);"></div>
                            </div>
                        </div>
                    </div>
                    <div style="display: flex; flex-direction: column; align-items: flex-end; justify-content: center; gap: 2px;">
                        <span class="metric-value" style="font-size: 0.85rem;" id="val-bat-volts">0.00V</span>
                        <span class="metric-label" style="font-size: 0.65rem;" id="val-bat-cells">0S (0.00V/cell)</span>
                    </div>
                </div>

                <!-- Status / RSSI Telemetry Row -->
                <div style="display: flex; justify-content: space-between; border-top: 1px solid var(--panel-border); padding-top: 12px; margin-top: 8px; gap: 16px;">
                    <!-- FC Connection Status & RSSI -->
                    <div style="display: flex; align-items: center; gap: 8px; flex: 1;">
                        <svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="color: var(--text-secondary);" id="fc-rssi-icon">
                            <path d="M5 12.55a11 11 0 0 1 14.08 0"></path>
                            <path d="M1.42 9a16 16 0 0 1 21.16 0"></path>
                            <path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path>
                            <line x1="12" y1="20" x2="12.01" y2="20"></line>
                        </svg>
                        <div style="display: flex; flex-direction: column; gap: 1px;">
                            <span class="metric-label" style="font-size: 0.7rem; line-height: 1;">Drone Link</span>
                            <span class="metric-value" style="font-size: 0.8rem; font-weight: bold; color: var(--accent-red);" id="val-fc-status">DISCONNECTED</span>
                        </div>
                    </div>
                    <div style="display: flex; flex-direction: column; align-items: flex-end; justify-content: center; gap: 1px;">
                        <span class="metric-value" style="font-size: 0.8rem;" id="val-fc-rssi">--- dBm</span>
                    </div>
                </div>
                
                <div style="display: flex; justify-content: space-between; border-top: 1px solid var(--panel-border); padding-top: 12px; margin-top: 8px; gap: 16px;">
                    <!-- Phone Connection Status & RSSI -->
                    <div style="display: flex; align-items: center; gap: 8px; flex: 1;">
                        <svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="color: var(--text-secondary);" id="phone-rssi-icon">
                            <rect x="5" y="2" width="14" height="20" rx="2" ry="2"></rect>
                            <line x1="12" y1="18" x2="12.01" y2="18"></line>
                        </svg>
                        <div style="display: flex; flex-direction: column; gap: 1px;">
                            <span class="metric-label" style="font-size: 0.7rem; line-height: 1;">Phone Link</span>
                            <span class="metric-value" style="font-size: 0.8rem; font-weight: bold; color: var(--accent-red);" id="val-phone-status">DISCONNECTED</span>
                        </div>
                    </div>
                    <div style="display: flex; flex-direction: column; align-items: flex-end; justify-content: center; gap: 1px;">
                        <span class="metric-value" style="font-size: 0.8rem;" id="val-phone-rssi">--- dBm</span>
                    </div>
                </div>
            </div>

            <!-- Trim Adjustment Controls -->
            <div class="telemetry-card" style="padding: 12px 16px; margin-top: -8px;">
                <div class="metric-label" style="text-align: center; margin-bottom: 8px;">Trim Controls</div>
                <div style="display: flex; justify-content: space-around; width: 100%; gap: 12px;">
                    <!-- Roll Trim -->
                    <div style="display: flex; flex-direction: column; align-items: center; gap: 4px;">
                        <span class="metric-label">Roll Trim</span>
                        <div style="display: flex; align-items: center; gap: 8px;">
                            <button onclick="changeRollTrim(-2)" class="btn-fullscreen" style="border-radius: 6px; width: 28px; height: 28px; font-weight: bold;">L</button>
                            <span id="txt-roll-trim" class="metric-value" style="font-size: 0.9rem; width: 32px; text-align: center;">0</span>
                            <button onclick="changeRollTrim(2)" class="btn-fullscreen" style="border-radius: 6px; width: 28px; height: 28px; font-weight: bold;">R</button>
                        </div>
                    </div>
                    <!-- Pitch Trim -->
                    <div style="display: flex; flex-direction: column; align-items: center; gap: 4px;">
                        <span class="metric-label">Pitch Trim</span>
                        <div style="display: flex; align-items: center; gap: 8px;">
                            <button onclick="changePitchTrim(-2)" class="btn-fullscreen" style="border-radius: 6px; width: 28px; height: 28px; font-weight: bold;">B</button>
                            <span id="txt-pitch-trim" class="metric-value" style="font-size: 0.9rem; width: 32px; text-align: center;">0</span>
                            <button onclick="changePitchTrim(2)" class="btn-fullscreen" style="border-radius: 6px; width: 28px; height: 28px; font-weight: bold;">F</button>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- Right Joystick (Pitch / Roll) -->
        <div class="joystick-outer" id="right-container">
            <div class="crosshair-h"></div>
            <div class="crosshair-v"></div>
            <span class="lbl-container lbl-top">Pitch F</span>
            <span class="lbl-container lbl-bottom">Pitch B</span>
            <span class="lbl-container lbl-left">Roll L</span>
            <span class="lbl-container lbl-right">Roll R</span>
            <div class="joystick-knob" id="right-knob"></div>
        </div>
    </div>

    <script>
        let socket;
        let isConnected = false;
        let isArmed = false;

        const sticks = {
            throttle: 1000,
            yaw: 1500,
            pitch: 1500,
            roll: 1500
        };

        let rollTrim = parseInt(localStorage.getItem('rollTrim')) || 0;
        let pitchTrim = parseInt(localStorage.getItem('pitchTrim')) || 0;

        // Initialize UI with saved values on load
        window.addEventListener('load', () => {
            document.getElementById('txt-roll-trim').innerText = (rollTrim > 0 ? '+' : '') + rollTrim;
            document.getElementById('txt-pitch-trim').innerText = (pitchTrim > 0 ? '+' : '') + pitchTrim;
        });

        function changeRollTrim(amt) {
            rollTrim = Math.max(-100, Math.min(100, rollTrim + amt));
            document.getElementById('txt-roll-trim').innerText = (rollTrim > 0 ? '+' : '') + rollTrim;
            localStorage.setItem('rollTrim', rollTrim);
        }

        function changePitchTrim(amt) {
            pitchTrim = Math.max(-100, Math.min(100, pitchTrim + amt));
            document.getElementById('txt-pitch-trim').innerText = (pitchTrim > 0 ? '+' : '') + pitchTrim;
            localStorage.setItem('pitchTrim', pitchTrim);
        }

        const statusDot = document.getElementById('status-dot');
        const statusText = document.getElementById('status-text');
        
        // UI Elements
        const valThr = document.getElementById('val-thr');
        const valYaw = document.getElementById('val-yaw');
        const valPit = document.getElementById('val-pit');
        const valRol = document.getElementById('val-rol');

        const barThr = document.getElementById('bar-thr');
        const barYaw = document.getElementById('bar-yaw');
        const barPit = document.getElementById('bar-pit');
        const barRol = document.getElementById('bar-rol');

        function connectWebSocket() {
            const host = window.location.hostname || '192.168.4.1';
            socket = new WebSocket(`ws://${host}:81`);

            socket.onopen = () => {
                isConnected = true;
                statusDot.classList.add('connected');
                statusText.innerText = 'Online';
                statusText.style.color = 'var(--accent-green)';
            };

            socket.onmessage = (event) => {
                const data = event.data.split(',');
                if (data[0] === 't') {
                    const voltage = parseFloat(data[1]);
                    const cellVoltage = parseFloat(data[2]);
                    const percentage = parseInt(data[3]);
                    const cells = parseInt(data[4]);
                    
                    document.getElementById('val-bat-pct').innerText = percentage + '%';
                    const barBat = document.getElementById('bar-bat');
                    barBat.style.width = percentage + '%';
                    
                    if (percentage < 20) {
                        barBat.style.backgroundColor = 'var(--accent-red)';
                    } else if (percentage < 50) {
                        barBat.style.backgroundColor = '#ff9500'; // orange
                    } else {
                        barBat.style.backgroundColor = 'var(--accent-green)';
                    }
                    
                    document.getElementById('val-bat-volts').innerText = voltage.toFixed(2) + 'V';
                    document.getElementById('val-bat-cells').innerText = cells + 'S (' + cellVoltage.toFixed(2) + 'V/cell)';
                    
                    if (data.length >= 9) {
                        const fcStatus = parseInt(data[5]);
                        const phoneStatus = parseInt(data[6]);
                        const fcRssi = parseInt(data[7]);
                        const phoneRssi = parseInt(data[8]);
                        
                        const fcStatusEl = document.getElementById('val-fc-status');
                        const fcRssiEl = document.getElementById('val-fc-rssi');
                        const phoneStatusEl = document.getElementById('val-phone-status');
                        const phoneRssiEl = document.getElementById('val-phone-rssi');
                        
                        if (fcStatus === 1) {
                            fcStatusEl.innerText = 'CONNECTED';
                            fcStatusEl.style.color = 'var(--accent-green)';
                            fcRssiEl.innerText = fcRssi + ' dBm';
                        } else {
                            fcStatusEl.innerText = 'DISCONNECTED';
                            fcStatusEl.style.color = 'var(--accent-red)';
                            fcRssiEl.innerText = '--- dBm';
                        }
                        
                        if (phoneStatus === 1) {
                            phoneStatusEl.innerText = 'CONNECTED';
                            phoneStatusEl.style.color = 'var(--accent-green)';
                            phoneRssiEl.innerText = phoneRssi + ' dBm';
                        } else {
                            phoneStatusEl.innerText = 'DISCONNECTED';
                            phoneStatusEl.style.color = 'var(--accent-red)';
                            phoneRssiEl.innerText = '--- dBm';
                        }
                    }
                }
            };

            socket.onclose = () => {
                isConnected = false;
                statusDot.classList.remove('connected');
                statusText.innerText = 'Offline';
                statusText.style.color = 'var(--text-secondary)';
                if (isArmed) disarmSystem();
                setTimeout(connectWebSocket, 2000);
            };

            socket.onerror = (err) => {
                console.error("WS Error:", err);
            };
        }

        // Tactile Slide-To-Arm Logic
        const sliderContainer = document.getElementById('slider-arm-container');
        const sliderHandle = document.getElementById('slider-arm-handle');
        const sliderText = document.getElementById('slider-arm-text');
        
        let sliderActive = false;
        let sliderStartX = 0;
        let maxSliderDistance = 0;

        function initSlider() {
            maxSliderDistance = sliderContainer.clientWidth - sliderHandle.clientWidth - 8;
        }

        window.addEventListener('resize', initSlider);
        setTimeout(initSlider, 100);

        sliderHandle.addEventListener('mousedown', startSliderDrag);
        sliderHandle.addEventListener('touchstart', startSliderDrag, { passive: false });
        window.addEventListener('mousemove', dragSlider);
        window.addEventListener('touchmove', dragSlider, { passive: false });
        window.addEventListener('mouseup', endSliderDrag);
        window.addEventListener('touchend', endSliderDrag);

        function startSliderDrag(e) {
            if (!isConnected) return;
            sliderActive = true;
            sliderStartX = e.touches ? e.touches[0].clientX : e.clientX;
            sliderHandle.style.transition = 'none';
            if (e.cancelable) e.preventDefault();
        }

        function dragSlider(e) {
            if (!sliderActive) return;
            const currentX = e.touches ? e.touches[0].clientX : e.clientX;
            let diffX = currentX - sliderStartX;

            if (!isArmed) {
                diffX = Math.max(0, Math.min(diffX, maxSliderDistance));
                sliderHandle.style.transform = `translateX(${diffX}px)`;
            } else {
                diffX = Math.min(0, Math.max(diffX, -maxSliderDistance));
                sliderHandle.style.transform = `translateX(${maxSliderDistance + diffX}px)`;
            }
            if (e.cancelable) e.preventDefault();
        }

        function endSliderDrag(e) {
            if (!sliderActive) return;
            sliderActive = false;
            sliderHandle.style.transition = 'transform 0.3s cubic-bezier(0.16, 1, 0.3, 1)';

            const currentTransform = new WebKitCSSMatrix(window.getComputedStyle(sliderHandle).transform).m41;

            if (!isArmed) {
                if (currentTransform > maxSliderDistance * 0.8) {
                    armSystem();
                } else {
                    sliderHandle.style.transform = 'translateX(0px)';
                }
            } else {
                if (currentTransform < maxSliderDistance * 0.8) {
                    disarmSystem();
                } else {
                    sliderHandle.style.transform = `translateX(${maxSliderDistance}px)`;
                }
            }
        }

        function armSystem() {
            isArmed = true;
            document.body.classList.add('armed-state');
            sliderContainer.classList.add('armed-bg');
            sliderText.innerText = 'Armed';
            sliderHandle.style.transform = `translateX(${maxSliderDistance}px)`;
            sendData();
        }

        function disarmSystem() {
            isArmed = false;
            document.body.classList.remove('armed-state');
            sliderContainer.classList.remove('armed-bg');
            sliderText.innerText = 'Slide to Arm';
            sliderHandle.style.transform = 'translateX(0px)';
            sendData();
        }

        // Altitude Hold Switch Logic
        let isAltHold = false;
        const btnAltHold = document.getElementById('btn-althold');
        btnAltHold.addEventListener('click', () => {
            isAltHold = !isAltHold;
            if (isAltHold) {
                btnAltHold.classList.add('active');
                btnAltHold.innerText = 'Altitude Hold: ON';
                leftJoystick.setCenterY(true);
                leftJoystick.reset();
            } else {
                btnAltHold.classList.remove('active');
                btnAltHold.innerText = 'Altitude Hold: OFF';
                leftJoystick.setCenterY(false);
                leftJoystick.resetToBottom();
            }
            sendData();
        });

        // Independent Multi-Touch Joystick Implementation
        const leftJoystick = setupJoystick('left-container', 'left-knob', false, true, (x, y) => {
            sticks.yaw = Math.round(1500 + x * 500);
            sticks.throttle = Math.round(1500 - y * 500);
            
            sticks.yaw = Math.max(1000, Math.min(2000, sticks.yaw));
            sticks.throttle = Math.max(1000, Math.min(2000, sticks.throttle));

            valThr.innerText = sticks.throttle;
            valYaw.innerText = sticks.yaw;

            barThr.style.width = `${((sticks.throttle - 1000) / 1000) * 100}%`;
            barYaw.style.width = `${((sticks.yaw - 1000) / 1000) * 100}%`;
        });

        setupJoystick('right-container', 'right-knob', true, true, (x, y) => {
            sticks.roll = Math.round(1500 + x * 150);
            sticks.pitch = Math.round(1500 - y * 150);

            sticks.roll = Math.max(1350, Math.min(1650, sticks.roll));
            sticks.pitch = Math.max(1350, Math.min(1650, sticks.pitch));

            valRol.innerText = sticks.roll;
            valPit.innerText = sticks.pitch;

            barRol.style.width = `${((sticks.roll - 1000) / 1000) * 100}%`;
            barPit.style.width = `${((sticks.pitch - 1000) / 1000) * 100}%`;
        });

        function setupJoystick(containerId, knobId, autoCenterY, enableLock, callback) {
            const container = document.getElementById(containerId);
            const knob = document.getElementById(knobId);
            let active = false;
            let touchId = null; // Stores the identifier for the finger active on this joystick
            
            let maxRadius = (container.clientWidth - knob.clientWidth) / 2;

            let currentX = 0;
            let currentY = autoCenterY ? 0 : maxRadius;

            // Locking states
            let dragStartX = 0;
            let dragStartY = 0;
            let lockAxis = null; // null, 'x', or 'y'
            const lockThreshold = 8; // pixels of movement before locking

            // Handle resizing of joysticks dynamically
            function recomputeBounds() {
                maxRadius = (container.clientWidth - knob.clientWidth) / 2;
                if (!active) {
                    currentX = 0;
                    currentY = autoCenterY ? 0 : maxRadius;
                    updateKnobPosition();
                }
            }
            window.addEventListener('resize', recomputeBounds);
            
            updateKnobPosition();

            container.addEventListener('mousedown', dragStart);
            container.addEventListener('touchstart', dragStart, { passive: false });
            
            window.addEventListener('mousemove', drag);
            window.addEventListener('touchmove', drag, { passive: false });
            
            window.addEventListener('mouseup', dragEnd);
            window.addEventListener('touchend', dragEnd);
            window.addEventListener('touchcancel', dragEnd);

            function dragStart(e) {
                if (e.touches) {
                    if (active) return; // Ignore secondary touches on same container
                    
                    const touch = e.changedTouches[0];
                    touchId = touch.identifier;
                    active = true;
                    knob.style.transition = 'none';
                    
                    dragStartX = touch.clientX;
                    dragStartY = touch.clientY;
                    lockAxis = null;
                    
                    handleMove(touch.clientX, touch.clientY);
                } else {
                    active = true;
                    knob.style.transition = 'none';
                    
                    dragStartX = e.clientX;
                    dragStartY = e.clientY;
                    lockAxis = null;
                    
                    handleMove(e.clientX, e.clientY);
                }
                if(e.cancelable) e.preventDefault();
            }

            function drag(e) {
                if (!active) return;
                
                if (e.touches) {
                    // Find the touch associated with this joystick
                    let touch = null;
                    for (let i = 0; i < e.touches.length; i++) {
                        if (e.touches[i].identifier === touchId) {
                            touch = e.touches[i];
                            break;
                        }
                    }
                    if (touch) {
                        handleMove(touch.clientX, touch.clientY);
                    }
                } else {
                    handleMove(e.clientX, e.clientY);
                }
                if(e.cancelable) e.preventDefault();
            }

            function dragEnd(e) {
                if (!active) return;
                
                if (e.touches) {
                    let touchActive = false;
                    for (let i = 0; i < e.touches.length; i++) {
                        if (e.touches[i].identifier === touchId) {
                            touchActive = true;
                            break;
                        }
                    }
                    if (!touchActive) {
                        active = false;
                        touchId = null;
                        lockAxis = null;
                        resetKnob();
                    }
                } else {
                    active = false;
                    lockAxis = null;
                    resetKnob();
                }
            }

            function resetKnob() {
                knob.style.transition = 'transform 0.4s cubic-bezier(0.16, 1, 0.3, 1)';
                currentX = 0;
                if (autoCenterY) {
                    currentY = 0;
                }
                updateKnobPosition();
                triggerCallback();
            }

            function handleMove(clientX, clientY) {
                const rect = container.getBoundingClientRect();
                const centerX = rect.left + rect.width / 2;
                const centerY = rect.top + rect.height / 2;

                let dx = clientX - centerX;
                let dy = clientY - centerY;

                // Determine directional lock if enabled
                if (enableLock && !lockAxis) {
                    const moveX = clientX - dragStartX;
                    const moveY = clientY - dragStartY;
                    const dist = Math.sqrt(moveX * moveX + moveY * moveY);
                    
                    if (dist > lockThreshold) {
                        if (Math.abs(moveY) > Math.abs(moveX)) {
                            lockAxis = 'y'; // Lock to vertical (Throttle only)
                        } else {
                            lockAxis = 'x'; // Lock to horizontal (Yaw only)
                        }
                    }
                }

                // Apply axis lock
                if (lockAxis === 'y') {
                    dx = 0; // Force X to center (Yaw = 1500)
                } else if (lockAxis === 'x') {
                    // Force Y to remain at its initial starting position
                    const startYRel = dragStartY - centerY;
                    dy = startYRel;
                }

                const dist = Math.sqrt(dx*dx + dy*dy);
                if (dist > maxRadius) {
                    currentX = (dx / dist) * maxRadius;
                    currentY = (dy / dist) * maxRadius;
                } else {
                    currentX = dx;
                    currentY = dy;
                }

                updateKnobPosition();
                triggerCallback();
            }

            function updateKnobPosition() {
                knob.style.transform = `translate(${currentX}px, ${currentY}px)`;
            }

            function triggerCallback() {
                const normX = currentX / maxRadius;
                const normY = currentY / maxRadius;
                callback(normX, normY);
            }

            return {
                setCenterY: (val) => {
                    autoCenterY = val;
                    recomputeBounds();
                },
                reset: () => {
                    resetKnob();
                },
                resetToBottom: () => {
                    currentX = 0;
                    currentY = maxRadius;
                    updateKnobPosition();
                    triggerCallback();
                }
            };
        }

        // Send Data loop (40Hz / 25ms)
        function sendData() {
            if (!isConnected) return;
            const finalRoll = Math.max(1000, Math.min(2000, sticks.roll + rollTrim));
            const finalPitch = Math.max(1000, Math.min(2000, sticks.pitch + pitchTrim));
            const payload = `${finalRoll},${finalPitch},${sticks.throttle},${sticks.yaw},${isArmed ? 2000 : 1000},${isAltHold ? 2000 : 1000}`;
            socket.send(payload);
        }

        setInterval(sendData, 25);

        // Fullscreen Toggle Logic with Hardware-Accelerated Ripple
        const btnFullscreen = document.getElementById('btn-fullscreen');

        function triggerPremiumTransition(e) {
            // 1. Spawn Ripple Wave at button center
            const rect = btnFullscreen.getBoundingClientRect();
            const ripple = document.createElement('div');
            ripple.className = 'ripple-wave';
            ripple.style.left = `${rect.left + rect.width / 2}px`;
            ripple.style.top = `${rect.top + rect.height / 2}px`;
            document.body.appendChild(ripple);

            // Remove ripple after animation finishes
            setTimeout(() => ripple.remove(), 800);

            // 2. Trigger Apple Scale Entrance on Layout
            const layout = document.querySelector('.main-layout');
            layout.classList.remove('apple-transition-active');
            void layout.offsetWidth; // Trigger reflow
            layout.classList.add('apple-transition-active');
        }

        btnFullscreen.addEventListener('click', (e) => {
            triggerPremiumTransition(e);
            
            if (!document.fullscreenElement) {
                if (document.documentElement.requestFullscreen) {
                    document.documentElement.requestFullscreen();
                } else if (document.documentElement.webkitRequestFullscreen) {
                    document.documentElement.webkitRequestFullscreen();
                } else if (document.documentElement.msRequestFullscreen) {
                    document.documentElement.msRequestFullscreen();
                }
            } else {
                if (document.exitFullscreen) {
                    document.exitFullscreen();
                } else if (document.webkitExitFullscreen) {
                    document.webkitExitFullscreen();
                } else if (document.msExitFullscreen) {
                    document.msExitFullscreen();
                }
            }
        });

        document.addEventListener('fullscreenchange', () => {
            if (document.fullscreenElement) {
                btnFullscreen.innerHTML = '<svg viewBox="0 0 24 24" width="16" height="16" stroke="currentColor" stroke-width="2.1" fill="none" stroke-linecap="round" stroke-linejoin="round"><path d="M4 14h6v6m10-6h-6v6M4 10h6V4m10 6h-6V4"></path></svg>';
            } else {
                btnFullscreen.innerHTML = '<svg viewBox="0 0 24 24" width="16" height="16" stroke="currentColor" stroke-width="2.1" fill="none" stroke-linecap="round" stroke-linejoin="round"><path d="M8 3H5a2 2 0 0 0-2 2v3m18 0V5a2 2 0 0 0-2-2h-3m0 18h3a2 2 0 0 0 2-2v-3M3 16v3a2 2 0 0 0 2 2h3"></path></svg>';
            }
            // Trigger recalculations on resize/fullscreen change
            setTimeout(initSlider, 150);
        });

        window.onload = () => {
            connectWebSocket();
            barThr.style.width = '0%';
            barYaw.style.width = '50%';
            barPit.style.width = '50%';
            barRol.style.width = '50%';
        };
    </script>
</body>
</html>
)rawhtml";
