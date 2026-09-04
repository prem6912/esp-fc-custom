# AGENTS.md

ESP-FC flight controller firmware — fork of [rtlopez/esp-fc](https://github.com/rtlopez/esp-fc) (upstream remote is configured). C++17 / Arduino / PlatformIO; targets ESP32 family + RP2040/RP2350.

## Existing agent rules

`.agents/rules/espfc-guidelines.md` is an always-on rule file: flash directly when asked to deploy changes, verify over serial afterwards, keep all I2C on Core 1. Read it before hardware work.

## Commands

`pio` is not on PATH on this machine — invoke the venv copy:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" <cmd>
```

- Build one target: `pio run -e esp32` — envs: esp32, esp32s3, esp32s2, esp32c3, esp8266, rp2040, rp2350
- Flash: `pio run -t upload -e esp32 --upload-port COM8`
- Unit tests: `pio test -e native`, single suite: `-f test_math`. **Requires gcc/g++ on PATH — not installed here**, so native tests only run in CI or Docker (`docker compose run --rm espfc pio test -e native`; Docker also unavailable locally). Firmware envs build fine locally.
- Static analysis: `pio check` (cppcheck)
- Formatting: `pio run -t check_format` (dry-run) / `pio run -e native -t format` — needs clang-format in PATH (also missing here)

CI (`.github/workflows/platformio.yml`) runs native tests first, then builds all 7 targets.

## Architecture

- Real firmware code lives in `lib/Espfc/src/` (Sensor/, Control/, Connect/, Device/, ...) plus sibling libs (`EscDriver`, `AHRS`, `Gps`, `EspWire`). `src/main.cpp` only boots: disables brownout/WDT, creates tasks.
- ESP32 multicore split (src/main.cpp:124): `gyroTask` pinned to **Core 1** (prio 24) runs the fast loop (`espfc.update()`); `pidTask` on **Core 0** drains `espfc.updateOther()` (input/MSP/WiFi). Never issue I2C from Core 0.
- Persistent config lives in EEPROM: bump `EEPROM_VERSION` in `lib/Espfc/src/Utils/Storage.h` whenever config layout changes (this fork bumped it to 0x02).
- `transmitter/` is a separate PlatformIO project (ESP32 TX with WebSocket UI) — built independently from that dir.
- Untracked local dirs: `betaflight-src/` (vendored reference code — don't edit/build), `build-tools/`, `.pio/`.

## Gotchas

- gnu++17 is forced via both `build_flags` and `build_unflags` in platformio.ini (PIO would default to c++11); add new flags to `[env]` so all targets inherit them.
- Dev preset `-DESPFC_DEV_PRESET_BRUSHED` is enabled in `[env] build_flags`.
- Post-flash verification scripts live in `scratch/*.py` (raw MSP over serial COM8 @ 115200).
- Style: `.clang-format` (LLVM-based, 2-space indent, 120 cols, Allman braces); formatting scope covers only `src/` and `lib/Espfc/src/`.
- PR bar per docs/development.md: unit tests passing + static analysis clean required; untested changes are rejected.
