---
trigger: always_on
description: Development, flashing, and multi-core architecture rules for the ESP32 ESPFC flight controller codebase.
---

# ESP32 ESPFC Flight Controller Guidelines

## 1. Automated Direct Flashing Rule
- Whenever the user requests code updates, fixes, or flash operations, **ALWAYS directly flash the board** without asking for confirmation:
  `pio run -t upload -e esp32 --upload-port COM8`
- After flashing, proactively verify core telemetry and sensor outputs using python MSP test scripts in `scratch/`.

## 2. Multi-Core I2C Concurrency Rules
- **Exclusive Core 1 I2C Ownership**: All physical I2C sensor communications (`MPU6500` Gyro/Accel, `VL53L0X` ToF Laser, `BMP280` Baro, `HMC5883L` Mag) must execute strictly on **Core 1** inside `SensorManager::read()`.
- **Zero I2C on Core 0**: Core 0 (which handles Wi-Fi, HTTP Web Server, and UDP) must **NEVER** issue I2C transactions (`_bus->read/write`). It must only process already-cached sensor state values.
- **I2C Multiplexing**: Time-slice secondary sensors (VL53L0X, Baro, Mag) so that at most one secondary sensor is queried per gyro cycle.

## 3. Real-Time Attitude Fusion & Sensor Loops
- **Zero-Latency IMU Fusion**: Run attitude fusion (`_sensor.fusion()`) immediately when fresh accelerometer data is processed on Core 1 rather than queueing frames across cores.
- **Queue Draining**: In FreeRTOS task loops (such as `pidTask` on Core 0), always drain the event queue completely before yielding:
  ```cpp
  while(espfc.updateOther()) { /* drain all events */ }
  vTaskDelay(1);
  ```
- **Attitude Fusion Fallback**: Always ensure `FUSION_MAHONY` is active and valid even if EEPROM configuration defaults to `FUSION_NONE`.

## 4. Hardware Watchdog & Safety Constraints
- **Watchdog Management**: Suppress `TIMERG0` and `TIMERG1` hardware watchdogs and brownout detector false alarms in `src/main.cpp` to prevent spurious resets during high-performance loop operation.
- **Wi-Fi Control**: Maintain pure SoftAP (`ESP32-DRONE` / `12345678`), Web Cockpit (`http://192.168.4.1`), UDP port 8888, and 1.5-second failsafe timeout (auto min-throttle and disarm on link loss).
