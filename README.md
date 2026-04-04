<p align="center">
  <img src="./screenshot.png" alt="ESP32 Sonar Bridge Web UI" width="1100">
</p>

# ESP32 Sonar Bridge

ESP32 firmware that bridges either a Deeper CHIRP+ Wi-Fi sonar or a hardwired GL04x-style UART sonar into MAVLink `DISTANCE_SENSOR`, built on top of DroneBridge ESP32.

## Why This Project Exists

This project replaces a split legacy setup with one ESP32:

- old Wi-Fi / MAVLink bridge on one microcontroller
- old hardwired sonar reader on a second microcontroller
- duplicated wiring, duplicated power, and duplicated maintenance

The goal is one board that can:

- read a hardwired underwater sonar
- optionally join a Deeper sonar over Wi-Fi
- publish depth to a flight controller over MAVLink
- keep a simple Web UI for configuration and live debug

## Current Capabilities

- Hardwired UART sonar reading at `115200 8N1`
- Deeper UDP/NMEA receive and parsing over Wi-Fi
- Boot-time source selection between Deeper and hardwired sonar
- MAVLink `DISTANCE_SENSOR` output to the flight-controller serial link and radio/TCP clients
- Web UI with sonar settings, depth panels, and debug views
- Persistent rolling sonar log with live preview, download, and clear controls
- Wi-Fi OTA for both `db_esp32.bin` and `www.bin`, with rollback-capable dual app slots
- Bench logging and debug surfaces for both sonar paths

## Boot Behavior

If Deeper fallback is enabled, the firmware uses a one-shot boot policy:

1. The ESP gets one `60 second` boot-time attempt to join the configured Deeper SSID.
2. If it connects, Deeper becomes the only active sonar source for that boot.
3. If it does not connect, the ESP returns to DroneBridge AP mode, enables the hardwired sonar, and does not retry Deeper until the next reboot.

This avoids mixing both sonar sources during the same boot and makes bench testing more deterministic.

## Hardware Notes

- Target board: classic ESP32
- Default hardwired sonar pins:
  - ESP `GPIO17` -> sonar `RX` / trigger
  - sonar `TX` / data -> ESP `GPIO16`
- If the hardwired sonar `TX` line is `5V` TTL, level-shift it before the ESP32 `RX` pin.
- The hardwired UART protocol currently implemented follows the collected manufacturer reference:
  - trigger byte `0xFF`
  - frame format `FF Data_H Data_L SUM`

## Flight Controller Wiring

Current live bench-tested MAVLink UART settings:

- ESP32 `GPIO12` -> FC `RX`
- FC `TX` -> ESP32 `GPIO14`
- ESP32 `GND` -> FC `GND`
- `RTS = 0`, `CTS = 0`, so flow control is disabled
- serial protocol = MAVLink
- baud = `57600`

Practical notes:

- this FC link is separate from the hardwired sonar UART on `GPIO17` / `GPIO16`
- if the FC `TX` line is `5V` TTL, level-shift it before the ESP32 `GPIO14` input
- most flight controllers use `3.3V` UART, but verify before wiring

## Project Layout

- `firmware/` - ESP-IDF firmware, Web UI, and build configuration
- `HARDWIRED_SONAR_REFERENCE.md` - collected vendor notes for the hardwired UART sonar
- `CURRENT_DBG_CONTEXT.md` - detailed debugging handoff and development history
- `FINISH_TODO.md` - remaining work before calling the project stable
- `archive/` - local-only backups, probe logs, and historical artifacts; ignored by Git

## Shared Knowledge

Cross-project RC and autopilot reference material now lives in `projects/rc_shared_knowledge/`.
The current live bait-boat system chain is documented in `projects/rc_shared_knowledge/docs/60_current_bait_boat_stack.md`.

## Build

This project uses ESP-IDF and CMake.

Typical flow:

```bash
cd firmware
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash
```

On Windows, a fully local build path is more reliable than building directly from a mapped or UNC path.

## OTA Update Procedure

As of `2026-04-04`, this project has been migrated to a `4MB` dual-slot OTA layout.
Normal firmware updates should now happen over Wi-Fi instead of opening the boat for serial flashing.

Current post-migration partition layout:

- `otadata` at `0xF000`
- `ota_0` at `0x20000`
- `ota_1` at `0x1A0000`
- `www` at `0x320000`
- `logs` at `0x380000`
- final `256K` left unallocated as reserve

Normal update flow:

1. Build from a fully local path, not directly from `X:` or a UNC path.
2. Collect `build/db_esp32.bin` for the firmware app update.
3. Collect `build/www.bin` only if the Web UI / SPIFFS content changed.
4. Power the boat and let the ESP either fall back to AP mode after the `60 second` Deeper probe or enter AP update mode from a previous maintenance session.
5. Connect to the ESP AP:
   - SSID: `DroneBridge for ESP32`
   - password: `dronebridge`
   - IP: `192.168.5.1`
6. Open `http://192.168.5.1/ota`
7. Upload `db_esp32.bin` to update the application.
8. Upload `www.bin` only when needed for Web UI changes.
9. Wait for the automatic reboot, then verify the board came back normally.

Important OTA notes:

- OTA app updates write to the inactive app slot and use rollback protection.
- The `/ota` page is embedded in the app binary, so recovery should still work even if the SPIFFS web partition is damaged.
- The `www.bin` upload must match the full SPIFFS partition size generated by the build.
- Persistent rolling sonar logs live in a separate `logs` partition, so normal `db_esp32.bin` and `www.bin` OTA updates do not erase them.
- If Deeper is enabled, the ESP may spend up to `60 seconds` trying the Deeper STA boot window before it falls back to AP mode.

When wired flashing is still required:

- bootloader changes
- partition table changes
- first-time OTA migration on a non-OTA board
- recovery if neither app slot boots far enough to serve `/ota`

Current wired flash layout after the OTA migration:

```text
0x1000   bootloader.bin
0x8000   partition-table.bin
0xF000   ota_data_initial.bin
0x20000  db_esp32.bin
0x320000 www.bin
flash_size = 4MB
```

The `2026-04-04` migration build was validated on real hardware on `COM13`.
See `OTA_MIGRATION_PLAN_2026-04-04.md` and `CURRENT_DBG_CONTEXT.md` for deeper implementation notes and history.

## Current Status

Bench and live FC-link validation completed:

- all four bench mode combinations were exercised:
  - both sonars off
  - hardwired only
  - Deeper only
  - both enabled, with Deeper winning when it connects during boot
- hardwired UART frames are parsed and converted to MAVLink `DISTANCE_SENSOR`
- Deeper UDP/NMEA data is parsed and converted to MAVLink `DISTANCE_SENSOR`
- Mission Planner / FC rangefinder validation is now confirmed for both sonar sources
- with Deeper active, the top Wi-Fi settings remain `DroneBridge for ESP32` / `dronebridge`
- saved AP credentials now remain stable in the Web UI even after a successful Deeper boot session
- Web UI sonar panels and debug views are live
- OTA migration to a `4MB` dual-slot layout completed and verified on hardware
- Wi-Fi OTA proof-of-concept completed for both the app and the Web UI
- the main page now shows the actual running firmware version and active OTA slot
- a dedicated `256K` persistent log partition is live, with downloadable rolling sonar logs exposed in the Web UI
- the earlier `httpd` and `Tmr Svc` stack overflows were fixed

Still open:

- longer soak/stability validation
- frontend polling / abort cleanup
- shutdown-time Deeper STA reconnect noise during intentional reboot

Known non-blocking quirk:

- some boot logs still print an ESP-IDF default AP startup line for `192.168.4.1`, even though the runtime AP remains on the configured `192.168.5.1`

See [FINISH_TODO.md](./FINISH_TODO.md) for the short list and [CURRENT_DBG_CONTEXT.md](./CURRENT_DBG_CONTEXT.md) for the detailed development log.

## Credits

- Based on DroneBridge ESP32
- Uses bench captures and vendor documentation for the hardwired sonar protocol
