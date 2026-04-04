# Current Debugging Context - ESP32 Sonar Bridge

## Latest Session Update
- Date: 05-04-2026
- This file is the compaction handoff for the current session. The older detailed notes below are still useful, but the bullets in this section are the fastest way to resume work.

### OTA Migration + Wi-Fi Update Validation - 04/05-04-2026
- The boat is no longer on the old single-app `2MB` layout.
- A one-time wired migration flash on `COM13` moved the device to a real `4MB` OTA-ready partition map with:
  - `otadata`
  - `ota_0`
  - `ota_1`
  - `www`
  - `logs`
  - final `256K` unallocated reserve
- The migration build, OTA recovery page, and rollback-capable dual-slot app flow were validated on hardware.
- `/ota` is embedded in the app image and remains available even if the `www` partition is damaged.
- Normal maintenance path is now:
  - connect to `DroneBridge for ESP32`
  - open `http://192.168.5.1/ota`
  - upload `db_esp32.bin` for firmware
  - upload `www.bin` for frontend changes
- One-time AP maintenance mode is implemented:
  - the `Reboot Into AP Update Mode` action sets an NVS flag
  - next boot skips the Deeper probe and comes up directly in AP mode
- Wi-Fi OTA proof-of-concept is complete:
  - `www.bin` OTA successfully changed frontend branding and button styling
  - `db_esp32.bin` OTA successfully switched the running app from `ota_0` to `ota_1`
  - main frontend now shows the real running app version/slot from `/api/update/info`
- Current app version in source is `1.0.1`
- Last verified live OTA status:
  - running partition `ota_1`
  - next update target `ota_0`

### Persistent Rolling Sonar Logs - 05-04-2026
- A dedicated `256K` SPIFFS `logs` partition is now part of the live flashed layout at `0x380000`.
- New backend logger files:
  - `firmware/main/db_sonar_log.c`
  - `firmware/main/db_sonar_log.h`
- Log storage path:
  - mount point `/logs`
  - file `/logs/sonar.log`
- New HTTP API:
  - `GET /api/logs/status`
  - `GET /api/logs/sonar`
  - `GET /api/logs/sonar/download`
  - `DELETE /api/logs/sonar`
- New main-page frontend block:
  - persistent log status readout
  - `Read / Refresh Log`
  - `Download Log`
  - `Clear Log`
- Current persistent log content includes:
  - boot events
  - hardwired frame issues / no-response cases
  - hardwired zero-run start / clear transitions
  - hardwired sampled frames
  - hardwired FC-facing publish snapshots
  - Deeper track samples with depth / temperature / GPS fields
- Important live bug and fix:
  - first logger implementation only persisted boot metadata plus later publish/state events
  - this made the downloaded file look stuck at the boot line while the hardwired debug window still updated
  - fix was to persist sampled hardwired frames directly from `danevi_sonar.c`
  - fix was rebuilt and pushed successfully over Wi-Fi
- Latest live verification:
  - `/api/logs/status` reports the log partition mounted successfully
  - downloaded log now grows continuously during hardwired traffic
  - frontend spacing around the persistent log controls was also adjusted and pushed over `www.bin` OTA

### Measured Log Retention - 05-04-2026
- The live unit was measured in the current hardwired worst-case debug state:
  - repeated `0 mm` frames
  - roughly one persistent `hardwired_frame` line per second
- Measured live numbers from the ESP:
  - usable log file budget: `225489` bytes
  - observed write rate: about `67.7 bytes/sec`
  - observed average line length: about `69.9 bytes`
  - observed line rate: about `0.97 lines/sec`
- Practical retention estimate at that exact live rate:
  - first trim from empty at about `55.5 minutes`
  - rolling history once trimming starts: about `42-56 minutes`
- Interpretation:
  - current retention is acceptable for the present debug phase
  - if longer history is needed later, the next optimization target is to log hardwired state changes/events plus a slow heartbeat instead of every sampled frame

### README / Operator Notes Updated - 04/05-04-2026
- `README.md` now documents the Wi-Fi OTA procedure, the current partition layout, and the dedicated persistent log capability.
- The project README is now safe to use as the first stop when asked how to update the boat without opening it.

### Agreed Hardwired Zero-Filter Plan - 15-03-2026
- User and agent agreed that hardwired sonar `0 mm` is not a useful FC depth value for this boat/mapping use case.
- Working interpretation:
  - `0 mm` should be treated as an invalid or unusable hardwired sample
  - likely causes include brief transducer exit from water, turbulence, bubbles, or lost bottom returns
  - `0 mm` should remain visible in raw debug/observability so the failure mode is not hidden
- Agreed firmware behavior target:
  - keep hardwired polling at `100 ms`
  - do **not** let `0 mm` overwrite the FC-facing hardwired depth immediately
  - hold the last good non-zero hardwired reading for a short grace window
  - if the zero run continues past the grace window, stop reporting hardwired depth instead of publishing `0`
  - resume immediately when a new non-zero hardwired frame arrives
- Agreed observability target:
  - preserve raw hardwired frame debug
  - add enough diagnostics to distinguish:
    - last raw hardwired reading
    - last good non-zero hardwired reading
    - consecutive zero count / zero-run state
- Practical workflow note:
  - user plans to leave a USB cable inside the boat on the next open/flash cycle, so future firmware iteration cost should be much lower
- Durable implementation plan for this work is tracked in:
  - `HARDWIRED_ZERO_FILTER_IMPLEMENTATION_PLAN_15-03-2026.md`
- Implementation progress update:
  - backend zero-filter slice is now implemented in `firmware/main/danevi_sonar.c` and `firmware/main/danevi_sonar.h`
  - current first-pass grace window is `600 ms`
  - `/api/system/stats` now exposes raw hardwired depth, last good hardwired depth, zero-run state, and zero-filter hold status
  - frontend hardwired summary now shows published depth, last raw reading, and filter status for easier lake/bench interpretation
  - firmware build passed in the known-good local mirror at `C:\Users\valen\esp32_sonar_build`
  - remaining practical work is flash + bench validation on hardware

### Deeper Water Temperature MAVLink Telemetry - 15-03-2026
- Added Deeper water temperature publishing to MAVLink from `firmware/main/db_timers.c`
- Current message choice is `NAMED_VALUE_FLOAT` with name `waterTemp`
- Publish policy:
  - only when `DB_ACTIVE_SONAR_SOURCE` is `DB_SONAR_SOURCE_DEEPER`
  - only when the Deeper snapshot still has a fresh temperature value
  - publish rate limited to about `1 Hz`
- Transport path matches the sonar publisher:
  - write to FC over serial
  - also forward to radio clients
- Important intent note:
  - this is an honest telemetry value, not a fake `SCALED_PRESSURE*` barometer message
  - practical validation still needed on the target FC/GCS to confirm how the receiving side displays or logs `waterTemp`

### Field Test Note - 14-03-2026
- Lake validation was performed with the hardwired sonar path only. Deeper was not tested in this run.
- End-to-end data path is now confirmed in real use for the hardwired sonar:
  - sonar -> ESP32
  - ESP32 -> flight controller
  - flight controller -> ELRS / RadioMaster TX16S backpack link
  - radio backpack -> phone
  - phone -> Carp Pilot app
- User-observed depth behavior on the lake:
  - near shore startup depth was about `0.2 m`
  - moving farther out, the depth increased to about `0.7 m`
  - after going deeper than that, the displayed depth dropped to `0` and stayed there for a while
  - while returning toward shore, the display started reporting again at about `0.7 m`
  - approaching shallow water again, the depth decreased back toward about `0.2 m`
- Current FC configuration note:
  - `RNGFND1_MAX = 7 m`
  - this makes a flight-controller max-range clamp at `0.7 m` unlikely
- Planned next field step:
  - repeat the lake test while monitoring all three views at the same time:
    - radio telemetry
    - phone app
    - ESP web page / hardwired debug panel
- Initial interpretation from the field behavior:
  - the hardwired telemetry chain did not appear to freeze
  - the ESP32 likely did not reboot-loop during the zero-depth interval because valid depth reporting resumed again without needing intervention
  - future debugging should focus first on why the hardwired sonar path can report a valid-looking `0` in deeper water instead of assuming an end-to-end link failure

### Repo / Build State
- Source-of-truth project path: `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge`
- Public repo: `https://github.com/valentindanev/esp32_sonar_bridge`
- Project status is now archived locally under `projects/archive/esp32_sonar_bridge` and should be treated as a live, field-validated project rather than active development.
- Current public/local HEAD is expected to include:
  - the `4MB` OTA migration
  - `/ota` recovery + app/web Wi-Fi update flow
  - frontend running-firmware version display
  - persistent rolling sonar logs
  - latest logger/source-path fixes and frontend polish
- Current app version in source: `1.0.1`
- Current working tree should be clean apart from ignored/local-only probe assets after the latest push.
- Local cleanup note:
  - root-level backups and probe/build logs are being archived under `archive/`
  - tracked next-step note lives in `FINISH_TODO.md`
  - `/archive/` is ignored by Git and is intended to stay local-only
  - the interrupted source-path leftovers from the archive move were preserved under `archive/move_cleanup_source_stub_20260312`
- `donors/` has been removed from GitHub history and force-pushed out of the public repo.
- `/donors/` is now explicitly ignored, so local donor references can remain on disk without being uploaded again.
- Working tree still has untracked local probe logs, but donor reference content is now local-only.
- Legacy Git-metadata backups from the donor/publish cleanup are now archived locally under:
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\archive\legacy_git_metadata_20260311\esp32_sonar_bridge_firmware_git_BACKUP_20260311`
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\archive\legacy_git_metadata_20260311\esp32_sonar_bridge_nested_git_BACKUP_20260311`
- Reliable local build mirror: `C:\Users\valen\esp32_sonar_build`
- Active hardware target: classic ESP32 on `COM13`
- Latest verified live app state:
  - firmware `1.0.1`
  - running partition `ota_1`
  - next OTA target `ota_0`
- Latest local release package prepared under:
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\archive\releases\v1.0.0`
- Latest public release:
  - `https://github.com/valentindanev/esp32_sonar_bridge/releases/tag/v1.0.0`
- Flashing reminder: this board often needs manual download mode (`hold BOOT`, `tap RESET/EN`, keep holding `BOOT` for about `1-2s`)
- Latest local step-1 build status:
  - `http_server.c` patched for HTTP stack usage reduction
  - build succeeded in `C:\Users\valen\esp32_sonar_build`
  - stack-fix build has now been flashed successfully to `COM13`
- Latest flashed local fixes:
  - `http_server.c` POST receive loops were corrected to read `total_len - cur_len` instead of `total_len` on every iteration
  - the GPS coordinates font-size fix is now included on the live device
  - `db_param_print_values_to_buffer()` was hardened to be bounded by caller-supplied buffer size instead of using unbounded `strcat()`
  - parameter dump buffers in `main.c` are now heap-backed via `calloc()` instead of using large local stack arrays
  - AP station-list refresh during shutdown is now guarded to suppress the old `ESP_ERR_WIFI_STOP_STATE` noise on intentional reboot
  - successful Deeper boot sessions now restore the saved main AP `SSID` / `wifi_pass` instead of leaving the Deeper credentials in the top Wi-Fi settings fields
  - latest build succeeded in `C:\Users\valen\esp32_sonar_build`
  - latest flash to `COM13` succeeded after syncing the corrected `main.c` into the local build mirror

### README Migration Notes
- On `11-03-2026`, `README.md` was rewritten to be GitHub-facing instead of session-facing.
- Detailed debug history, architecture rationale, and bench-test notes now belong here in `CURRENT_DBG_CONTEXT.md`.
- Public README scope is now:
  - project purpose
  - current capabilities
  - boot behavior
  - hardware/build overview
  - short status summary
- Developer-only material such as probe logs, crash history, donor findings, and recovery notes should stay here or under `archive/`.

### What Was Completed In This Session
- Created and published the project GitHub repo
- Added Deeper frontend improvements:
  - smaller Deeper readout boxes
  - GPS coordinates field
- Added hardwired-sonar observability:
  - hardwired depth readout
  - hardwired debug panel
  - `Enable Hardwired Sonar` setting and UI switch
- Exposed `active_sonar_source` in `/api/system/stats`
- Implemented Valentin's required boot policy in firmware and documented it in `README.md`
- Built and flashed the boot-policy firmware successfully to `COM13`
- Reworked sonar MAVLink publishing so the FreeRTOS timer callback only wakes a dedicated task instead of encoding and sending `DISTANCE_SENSOR` inside `Tmr Svc`
- Built and flashed the dedicated-sonar-task firmware successfully to `COM13`

### Required Boot Policy Now Implemented
1. On boot, if Deeper fallback is enabled, the ESP gets one `60 second` Deeper connection window.
2. If Deeper connects in that window, the hardwired sonar stays off for that boot and MAVLink `DISTANCE_SENSOR` comes only from Deeper.
3. If Deeper does not connect in that window, the ESP falls back once to AP mode, stops trying Deeper until the next reboot, starts the hardwired sonar, and MAVLink `DISTANCE_SENSOR` comes only from the hardwired sonar.
4. Sonar source selection is fixed for the current boot.

### Latest Verified Runtime
- Latest verified build size after the boot-policy work: `0x11dae0`
- Latest flash to classic ESP32 on `COM13` succeeded on `11-03-2026`
- Latest dedicated-sonar-task build size: `0x11dcb0`
- Latest Deeper/AP-credential-fix app SHA seen in boot log: `fb98d5e1b116d5ad...`
- Serial logs after the final flash confirmed the new one-shot Deeper boot window:
  - `Retry to connect to the AP (...) within boot window (60000 ms)`
  - `Timed out after 60000 ms while trying to connect to SSID: Deeper CHIRP+ 3B6D`
  - `STA Mode failed (Deeper not found). Falling back to normal AP Mode.`
  - `Deeper boot probe failed. Staying in AP mode and using hardwired sonar until the next reboot.`
- After that timeout/fallback:
  - the AP comes back
  - the hardwired sonar task starts
  - Deeper retries stop until the next reboot
- New post-fix verification log:
  - `sonar_task_boot_probe_20260311_a.txt`
  - shows `DB_TIMERS: Starting dedicated sonar MAVLink publish task.`
  - shows repeated valid hardwired frames such as `FF 00 1A 19 -> 26 mm`
  - shows repeated `Publishing hardwired sonar DISTANCE_SENSOR: 26 mm (2 cm)`
  - did **not** reproduce `***ERROR*** A stack overflow in task Tmr Svc has been detected.` during sustained hardwired publishing
- Save/reboot pipeline is now verified on the live device:
  - with both sonars disabled, `Save Settings & Reboot` now completes a normal NVS save and intentional reboot
  - no `Guru Meditation`
  - no heap assert in `cJSON_Delete`
  - no `main` task stack overflow
  - no app-level `ESP_ERR_WIFI_STOP_STATE` warning during the AP shutdown that precedes reboot
- Bench mode matrix now verified on live hardware:
  - both sonars disabled
  - hardwired enabled, Deeper disabled
  - hardwired disabled, Deeper enabled
  - both enabled
- Latest verified behavior for the two sonar-specific boot modes:
  - hardwired-only boot:
    - `ss_hardwired_en: 1`
    - `ss_deeper_en: 0`
    - hardwired task starts and valid UART frames are published as MAVLink `DISTANCE_SENSOR`
  - both enabled:
    - `ss_hardwired_en: 1`
    - `ss_deeper_en: 1`
    - if Deeper connects during boot, `Deeper selected at boot. Hardwired sonar stays off.`
    - only Deeper `DISTANCE_SENSOR` messages are then published for that boot
- The Deeper/AP credential leak is now fixed and verified live:
  - top Wi-Fi settings in the Web UI stay:
    - `SSID = DroneBridge for ESP32`
    - `Password = dronebridge`
  - the Deeper-specific fields still show:
    - `Deeper Sonar SSID = Deeper CHIRP+ 3B6D`
    - open password placeholder
  - this remains true even after a successful Deeper boot session with both sonar options enabled
- Live FC/GCS rangefinder validation is now confirmed on the real vehicle wiring:
  - ESP Web UI, Mission Planner, and the mobile app all showed matching hardwired depth on the same bench run
  - Mission Planner exposed the active feed as `rangefinder1`
  - with both sonar options enabled, Deeper-only publishing was confirmed after a successful Deeper boot connect
  - latest Deeper FC validation showed `Depth = 0.82 m` in the ESP Web UI and `rangefinder1 = 82` in Mission Planner on the same run
- Remaining shutdown noise during a successful save/reboot:
  - low-level ESP-IDF log line `wifi:NAN WiFi stop`
  - this appears to be framework noise during Wi-Fi teardown, not a project-level fault
- Important interpretation for future debugging:
  - if Deeper fallback is enabled and the sonar is not found, the AP disappearing for about the first `60 seconds` after boot is expected behavior now
  - this is not a reboot loop

### Current Known Open Items
- The frontend fetch noise (`signal is aborted without reason`) is still open. Likely cause: `frontend/dronebridge.js` creates an `AbortController` timeout in `get_json()` and never clears it after successful fetches, while `/api/system/stats` polling continues every `500 ms`.
- During intentional reboot while currently connected to Deeper in STA mode, the disconnect handler still logs:
  - `ESP_ERR_WIFI_NOT_STARTED ... esp_wifi_connect()`
  - this is shutdown-time reconnect noise, not a reboot failure
- Deeper still often fails to join during desk tests with `reason: 201` if the sonar is not fully awake / advertising in water mode.
- Longer soak validation is still pending.
- If no hardwired sensor is connected, repeated `DANEVI_SONAR: No hardwired sonar response within 100 ms` warnings are expected and not a firmware regression.
- The hardwired toggle is no longer an unconditional master switch. Current behavior is intentionally subordinate to the boot policy:
  - if the Deeper boot probe succeeds, hardwired stays off for that boot
  - if the Deeper boot probe fails, hardwired is started for that boot even if the saved hardwired toggle is off
- The hardwired parser mismatch is fixed in source and flashed:
  - `danevi_sonar.c` now follows the manufacturer UART format:
    - trigger `0xFF`
    - `4` byte response `FF Data_H Data_L SUM`
    - checksum `(0xFF + Data_H + Data_L) & 0xFF`
  - the old `ERR misaligned frame len=4 ...` behavior is gone in current runtime logs
- The earlier `Tmr Svc` stack overflow root cause is addressed in source and flashed:
  - `db_timers.c` moved sonar MAVLink encode/write/send work into a dedicated task with its own MAVLink status state and buffer
  - the 10 Hz timer now only wakes that task
  - latest runtime log shows the fix holding under real hardwired data
- Known non-blocking quirk:
  - ESP-IDF still logs a default AP startup line for `192.168.4.1` before the configured runtime AP settles on `192.168.5.1`
  - this has not broken the live Web UI path and is currently left as a documented upstream/framework-style quirk

### Hardware Notes For The Next Session
- Current FC-link UART settings seen in live bench tests:
  - ESP32 `GPIO12` = telemetry `TX` toward FC `RX`
  - ESP32 `GPIO14` = telemetry `RX` from FC `TX`
  - `RTS = 0`, `CTS = 0`, so flow control is disabled
  - serial protocol = MAVLink
  - baud = `57600`
- Default hardwired sonar pins in firmware:
  - ESP TX to sonar RX/trigger: `GPIO17`
  - sonar TX/data to ESP RX: `GPIO16`
- Current firmware now assumes the manufacturer-style UART protocol:
  - ESP sends `0xFF`
  - sonar answers with a `4-byte` frame `FF Data_H Data_L SUM`
- Hardwired-sensor temperature support exists only through the short Modbus boot window and represents internal compensation temperature, not water temperature.
- Voltage caution:
  - if the sonar TX line is `5V` TTL, level-shift it before feeding ESP32 `GPIO16`

### Recommended Next Steps
1. Fix the frontend polling / abort noise in `frontend/dronebridge.js`.
2. Suppress the Deeper STA reconnect attempt during intentional reboot so the shutdown log no longer emits `ESP_ERR_WIFI_NOT_STARTED`.
3. Run a longer soak test with the Web UI open and both sonar modes exercised.
4. Validate end-to-end delivery to a real flight controller for both sonar sources.
5. Keep the AP startup `192.168.4.1` log quirk documented, but do not prioritize it unless it becomes a real connectivity problem.

## Earlier Session Notes
- Date: 11-03-2026
- ESP32 implementation milestone completed and validated live on `COM13`:
  - dedicated Deeper UDP task now binds local UDP `10110`
  - sends `"$DEEP230,1*38\r\n"` to `192.168.10.1:10110`
  - receives live NMEA and writes it to the WebUI debug panel
  - parses:
    - `$SDDBT` -> depth
    - `$YXMTW` / `*MTW` -> water temperature
    - `*GGA` -> satellite count and GPS fix status
- The WebUI now shows human-readable Deeper fields above the raw NMEA box:
  - depth
  - water temperature
  - satellites / GPS fix
- Live HTTP stats verification after flash:
  - ESP32 current IP on Deeper subnet: `192.168.10.3`
  - example live stats:
    - `deeper_depth_mm = 0`
    - `deeper_temp_c_tenths = 314`
    - `deeper_satellites = 0`
    - `deeper_gps_fix = 0`
  - raw NMEA still visible in `deeper_debug`
- MAVLink output verification without a physical FC is now confirmed:
  - a TCP client connected to the ESP32 telemetry port `5760`
  - 5-second live capture result:
    - `51` packets of MAVLink `msgid 132` (`DISTANCE_SENSOR`)
    - `5` packets of `msgid 0` (`HEARTBEAT`)
    - `5` packets of `msgid 109` (`RADIO_STATUS`)
  - this proves the ESP32 is now emitting Deeper-derived `DISTANCE_SENSOR` messages successfully
- Important hardware/build note:
  - this current ESP32 build uses `CONFIG_DB_SERIAL_OPTION_UART=y`
  - serial telemetry is on `UART1` pins, not on the USB port
  - so we cannot use the USB port as a fake FC on this classic ESP32 build the way we did on the D1 mini
  - however, outgoing MAVLink can still be tested reliably over TCP/UDP on the radio side
- Critical architectural finding from live D1 mini validation:
  - the public `mavesp8266_deeper` donor and the public GitLab repo are not a Deeper-to-MAVLink depth parser
  - they are primarily a MAVLink bridge that can join the sonar Wi-Fi as a station so the tablet can use one Wi-Fi network for both sonar and telemetry
  - this explains why the donor is widely used while still not solving the boat's required "Deeper depth to FC" use case by itself
- A real `1.2.3` binary was obtained and tested:
  - file: `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\archive\binary_references\mavesp-esp12e-1.2.3.bin`
  - flashed successfully to a D1 mini on `COM4`
  - confirmed binary identity:
    - AP SSID: `Mavesp.1.2.3`
    - password: `12345678`
    - station password support extended to 24 chars in this fork
- Live network validation with the D1 mini:
  - Deeper AP: `Deeper CHIRP+ 3B6D`
  - Deeper AP IP: `192.168.10.1`
  - D1 mini joined as STA and served its web UI on `192.168.10.2`
  - laptop joined the same subnet as `192.168.10.3`
- Official Deeper UDP/NMEA path is now confirmed by both live test and Deeper support docs:
  - bind locally to UDP port `10110`
  - send `"$DEEP230,1*38\r\n"` to `192.168.10.1:10110`
  - Deeper then streams NMEA once per second
- Direct NMEA capture from the Deeper was successful from the laptop:
  - examples captured:
    - `$SDDBT,0.00,f,0.00,M,0.00,F*36`
    - `$YXMTW,31.40,C,*08`
    - `$GNRMC...`
    - `$GNGGA...`
    - `$GNVTG...`
  - desk test showed `0.00` depth, which is expected while the sonar is not in water
- Serial/FC-side validation on the D1 mini:
  - the laptop successfully impersonated the FC over `COM4`
  - after sending a fake MAVLink heartbeat, the D1 mini emitted binary MAVLink frames on the serial side
  - captured frames decoded as MAVLink `msgid 109` (`RADIO_STATUS`)
  - raw NMEA was not emitted on the serial/FC side
  - no MAVLink `DISTANCE_SENSOR` packet has been captured from the donor binary so far
- Practical conclusion for `esp32_sonar_bridge`:
  - the donor is only valid as proof of the Wi-Fi topology and STA/AP fallback approach
  - the actual Deeper depth feature still needs to be implemented in ESP32 firmware
  - next implementation target is now well defined:
    1. dedicated ESP32 Deeper UDP task
    2. bind UDP `10110`
    3. send `DEEP230` request
    4. display received raw NMEA in the WebUI debug text box
    5. parse `$SDDBT`
    6. inject MAVLink `DISTANCE_SENSOR`
- Date: 10-03-2026
- Source files updated on the mapped project copy and synced to the local build mirror:
  - `firmware/main/db_parameters.c`
  - `firmware/main/http_server.c`
  - `firmware/main/db_esp32_control.c`
  - `firmware/main/main.c`
- Backup copies were created before editing:
  - `firmware/main/*_BACKUP_20260310.c`

### Software-only fixes applied
- Fixed a real string assignment overflow in the settings path:
  - `db_param_is_valid_assign_str()` now copies using the target parameter's own max length and forces null termination.
- Hardened REST handlers against malformed requests:
  - `/api/settings` now rejects invalid JSON.
  - `/api/settings/clients/udp` now rejects invalid JSON and invalid IP/port input instead of using uninitialized data.
  - `/api/settings/clients/clear_udp` now returns an explicit success response.
- Fixed more Deeper runtime-mode mismatches:
  - internal telemetry socket setup now keys off actual ESP-IDF runtime STA mode instead of only configured `DB_PARAM_RADIO_MODE`
  - internal telemetry parsing is now bounded by the received packet length
  - REST system stats now report STA/AP information from the actual runtime Wi-Fi mode
  - static-IP application now keys off runtime STA mode
- Simplified and hardened the Deeper fallback restore path:
  - the AP SSID/password are now preserved in RAM before the Deeper probe and restored directly on STA failure
  - this removes the need for a second NVS read in the fallback path
- Removed one invalid-handle risk:
  - `db_read_settings_nvs()` no longer calls `nvs_close()` after a failed `nvs_open()`
- Added a dedicated Deeper password setting:
  - new saved parameter: `ss_deeper_pass`
  - the Deeper fallback path now uses this password when present and still supports open APs when it is empty
- Added a Deeper packet debug panel path for the WebUI:
  - recent TCP/UDP packet previews received while running in STA mode are now retained in RAM
  - `/api/system/stats` now exposes that text as `deeper_debug`
  - the web page now shows a read-only debug window under the Deeper settings section

### Build verification after fixes
- The patched source was synced to:
  - `C:\Users\valen\esp32_sonar_build`
- Build command succeeded:
  - `cmd /c ""C:\Users\valen\esp-idf-temp\export.bat" && "C:\Users\valen\.espressif\python_env\idf5.2_py3.13_env\Scripts\python.exe" "C:\Users\valen\esp-idf-temp\tools\idf.py" build"`
- Result:
  - `db_esp32.bin` built successfully
  - app size now `0x123250`

### New structural finding
- The current firmware still does not contain a Deeper payload decoder/injector.
- Evidence from source:
  - `danevi_sonar_set_distance()` is only written by the hardwired sonar task
  - the Deeper-side network path currently only logs packet previews in STA mode
- Practical meaning:
  - even if the ESP32 joins the Deeper AP successfully, current source does not yet prove that Deeper sonar depth is decoded and forwarded as MAVLink `DISTANCE_SENSOR`
- Next live step remains:
  - capture real Deeper TCP/UDP traffic while connected and implement the actual decoder path
  - hardwired sonar live testing is still blocked because the sensor is not physically connected right now

## Progress So Far
- The dual-core architecture is implemented.
  - DroneBridge networking runs on Core 0.
  - Hardwired sonar runs on Core 1.
- The 15-second Deeper fallback state machine is implemented.
- The Wi-Fi STA credential corruption bug is fixed.
- The STA failure to AP fallback crash is fixed.
- The saved Deeper SSID in NVS is now `Deeper CHIRP+ 3B6D`.
- The latest firmware and SPIFFS frontend were built and flashed successfully to the ESP32 on `COM13`.

## Build / Flash Notes
- Reliable build path on this Windows machine:
  - `C:\Users\valen\esp32_sonar_build`
- Share / mapped project path for source of truth:
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge`
- Building directly from UNC or mapped-share paths is unreliable because ESP-IDF frontend steps still resolve back to UNC paths.

## Latest Confirmed Runtime Behavior
Latest captured boot log after flashing the current diagnostic build:

```text
I (783) DB_ESP32:
	ssid: DroneBridge for ESP32
	wifi_pass: dronebridge
	...
	ss_deeper_en: 1
	ss_deeper_ssid: Deeper CHIRP+ 3B6D
...
I (834) DB_ESP32: Deeper Sonar Fallback Enabled. Temporarily switching to STA to scan for Deeper AP...
I (954) DB_ESP32: Using open security for Deeper STA connection
I (961) DB_ESP32: Forcing 802.11b/g/n compatibility for Deeper STA connection
I (1102) DB_ESP32: WIFI_EVENT_STA_START - Wifi Started
I (1105) DB_ESP32: Init of WiFi Client-Mode finished. (SSID: Deeper CHIRP+ 3B6D PASS: <open>)
I (1097) wifi:connected with Deeper CHIRP+ 3B6D, aid = 1, channel 1, BW20, bssid = e8:e8:b7:01:3b:6d
I (1102) wifi:security: Open Auth, phy: bg, rssi: -28
I (2135) DB_ESP32: IP_EVENT_STA_GOT_IP:192.168.10.2
I (2139) DB_ESP32: Connected to ap SSID:Deeper CHIRP+ 3B6D password:<open>
I (2146) DB_ESP32: WiFi client mode enabled and connected!
I (2152) DB_ESP32: Deeper Fallback SUCCESS! We are now connected to the Deeper Sonar AP.
ESP_ERROR_CHECK_WITHOUT_ABORT failed: esp_err_t 0x3005 (ESP_ERR_WIFI_MODE)
file: "./main/db_esp32_control.c" line 749
func: control_module_udp_tcp
expression: esp_wifi_ap_get_sta_list(&wifi_sta_list)
```

## Current Issue
The Deeper AP connection path is now working. The current remaining issue on the flashed firmware is STA-mode runtime code still calling an AP-only Wi-Fi API.

### What we know
- Windows scan result from `netsh wlan show networks mode=bssid`:
  - `SSID: Deeper CHIRP+ 3B6D`
  - `Authentication: Open`
  - `Encryption: None`
  - `Channel: 1`
  - `Signal: 81%`
- Runtime now confirms:
  - successful association to `Deeper CHIRP+ 3B6D`
  - open auth
  - RSSI around `-28`
  - DHCP lease `192.168.10.2`
- New repeated warning:
  - `ESP_ERR_WIFI_MODE` in `db_esp32_control.c` line `749`
  - failing API: `esp_wifi_ap_get_sta_list(&wifi_sta_list)`

### Interpretation
- The ESP sees the correct saved SSID.
- The ESP enters STA mode correctly.
- The ESP now uses open security correctly for the Deeper target.
- The security mismatch bug is resolved.
- The Deeper connection itself is now successful.
- The remaining bug is that control-path code still assumes AP mode after connecting in STA mode and repeatedly calls an AP-only ESP-IDF function.

## Latest Code Change Applied
- The source has now been patched further so runtime decisions can use the actual ESP-IDF Wi-Fi mode instead of only `DB_PARAM_RADIO_MODE`.
- New debug logging was also added:
  - inbound TCP payload previews in STA mode
  - inbound UDP payload previews in STA mode
  - throttled hardwired sonar frame logs
  - periodic hardwired `DISTANCE_SENSOR` publish logs
- The first logging build then exposed a stack overflow in the `danevi_sonar` task.
- The source was patched again to increase the sonar task stack from `2048` to `4096`.
- Files updated:
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\firmware\main\db_esp32_control.c`
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\firmware\main\db_timers.c`
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\firmware\main\danevi_sonar.c`
  - mirrored in `C:\Users\valen\esp32_sonar_build\main\...`
- Earlier in this session, `main.c` was also patched so the Deeper fallback path treats the target as an open AP.

## Latest Build / Flash Status
- Build status:
  - `idf.py build` succeeded in `C:\Users\valen\esp32_sonar_build`
  - latest app size: `0x122b30`
- Flash status:
  - the follow-up build with the enlarged sonar-task stack has now been flashed successfully
  - the previous `danevi_sonar` stack overflow is gone

## Latest Runtime Result After Stack Fix Flash
- Boot completed normally.
- `danevi_sonar` task no longer overflows or reboots the board.
- In this latest run, the ESP did not connect to Deeper:
  - repeated `WIFI_EVENT_STA_DISCONNECTED ... (reason: 201)`
  - then clean fallback to AP mode
- In this latest run, no repeated `ESP_ERR_WIFI_MODE` warnings were seen.
- In this latest run, no Deeper packet-preview logs appeared.
- In this latest run, no hardwired-sonar frame logs appeared.

## Source File Under Active Work
- `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\firmware\main\db_esp32_control.c`
- `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\firmware\main\db_timers.c`
- `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\firmware\main\danevi_sonar.c`
- Mirrored build copy:
  - `C:\Users\valen\esp32_sonar_build\main\...`

## Next Steps for the Next Session
1. Re-test with the Deeper sonar definitely awake and advertising in water-mode.
2. If Deeper connects again, watch for the new TCP/UDP packet-preview logs to identify its traffic path and payload format.
3. Verify whether the previous `ESP_ERR_WIFI_MODE` warning is truly resolved during a successful STA session.
4. Check the hardwired sonar wiring / activity if no `DANEVI_SONAR` frame logs appear during a debug window.

---

## Update 2026-03-11 16:30 - AP Stability Re-Test After HTTP Stack Fix

### Live Test Setup
- Host connected successfully to `DroneBridge for ESP32`.
- ESP serial console was available again on `COM13`.
- New combined capture log:
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\http_ping_probe_20260311_h_live_connected.txt`

### Verified Result
- The AP stayed connected and stable during:
  - `60` pings to `192.168.5.1`
  - manual `GET /`
  - manual `GET /api/system/info`
  - manual `GET /api/settings`
  - manual `GET /api/system/stats`
  - another `20` pings after the HTTP requests
- Ping results:
  - first window: `60 sent / 60 received / 0 lost`
  - second window: `20 sent / 20 received / 0 lost`
- No reboot, no panic, no `Guru Meditation`, and no `httpd` stack overflow appeared in serial.

### Important Conclusion
- The previous fatal web-triggered crash (`***ERROR*** A stack overflow in task httpd has been detected.`) appears fixed by the latest `http_server.c` patch.
- The ESP can now survive page/API traffic without dropping off Wi-Fi.

### Remaining Web Issue
- During `GET /`, serial still logs:
  - `httpd_sock_err: error in send : 104`
  - `DB_HTTP_REST: File sending failed!`
  - `500 Internal Server Error - Failed to send file`
  - `httpd_uri: uri handler execution failed`
- Despite those messages:
  - the client still got HTTP `200` on `/` in the live repro
  - API endpoints all returned HTTP `200`
  - Wi-Fi stayed up
- So the next HTTP issue is now narrowed to root-page/static-file sending behavior, not AP stability or task-stack overflow.

### Hardwired Sonar Note From Same Log
- Repeated warnings continue:
  - `DANEVI_SONAR: Hardwired sonar misaligned frame len=4 bytes=FF 00 00 FF 00`
- This reinforces the earlier conclusion that the current hardwired parser/trigger assumptions likely still do not match the real UART sonar protocol.

## Update 2026-03-11 16:35 - Root Page Send Failure Triage

### What was verified
- The earlier `GET /` repro that logged:
  - `DB_HTTP_REST: File sending failed!`
  - `500 Internal Server Error - Failed to send file`
  - `httpd_uri: uri handler execution failed`
  was caused by the test client reading only the first `512` bytes of `/` and then closing the socket.
- `firmware/frontend/index.html` is much larger than that:
  - file size on disk: `22463` bytes
- A full-body retest was run with serial active:
  - log file:
    - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\http_root_fullread_probe_20260311_a.txt`
- In that full-body test:
  - client received HTTP `200`
  - client read `54016` bytes
  - serial logged `DB_HTTP_REST: File sending complete`
  - no `File sending failed!`
  - no `500 Internal Server Error - Failed to send file`
  - no `httpd_uri: uri handler execution failed`

### Interpretation
- The three log lines above are one cascade caused by an early client disconnect while the server is still chunking the file.
- This is not the same issue as the earlier fatal `httpd` stack overflow.

### Source change now prepared
- `firmware/main/http_server.c` was patched so the static-file handler no longer escalates a broken client socket into a fake HTTP `500` on the server side.
- New behavior on chunk-send failure:
  - close the file
  - log a warning:
    - `Client disconnected while sending file: ...`
  - stop handling the request cleanly
- This should suppress the misleading `500` and `uri handler execution failed` noise for client-abort cases.

### Build / Flash state
- The patch builds successfully in:
  - `C:\Users\valen\esp32_sonar_build`
- The patch has now been flashed successfully to `COM13`.

### Post-flash verification
- New repro log:
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\http_root_partialread_probe_20260311_c_after_patch_ap_visible.txt`
- Re-tested the exact early-disconnect case:
  - client connected to `DroneBridge for ESP32`
  - client requested `/`
  - client intentionally read only `512` bytes and closed early
- New serial behavior:
  - `httpd_sock_err: error in send : 104`
  - `DB_HTTP_REST: Client disconnected while sending file: /www/index.html`
  - `httpd_sock_err: error in recv : 104`
- The old misleading chain is gone:
  - no `DB_HTTP_REST: File sending failed!`
  - no `500 Internal Server Error - Failed to send file`
  - no `httpd_uri: uri handler execution failed`

### Conclusion
- The first problem is fixed.
- The second and third messages were downstream noise from the same client-abort path and are also eliminated by this patch.

## Update 2026-03-11 17:00 - Hardwired Sonar Switched To Manufacturer UART Spec

### Source change applied
- `firmware/main/danevi_sonar.c` was patched away from the donor-style parser and toward the manufacturer-documented UART protocol.
- Main changes:
  - trigger byte changed from `0x55` to `0xFF`
  - expected response frame changed from `5` bytes to `4` bytes
  - checksum changed to include the header byte:
    - `(0xFF + Data_H + Data_L) & 0xFF`
  - distance decode kept as:
    - `distance_mm = Data_H * 256 + Data_L`
  - debug output now prints the actual received frame length and bytes more clearly
- Backup created first:
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\backups\firmware\main\danevi_sonar_BACKUP_20260311_manufacturer_uart4.c`

### Build / flash
- Build succeeded in:
  - `C:\Users\valen\esp32_sonar_build`
- Flash succeeded to:
  - `COM13`

### Verification result
- New logs:
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\hardwired_spec_boot_probe_20260311_a.txt`
  - `X:\backup\valentin\AI-Lab\projects\archive\esp32_sonar_bridge\hardwired_spec_runtime_probe_20260311_b.txt`
- The previous `ERR misaligned frame len=4 ...` issue is gone.
- The ESP now accepts and logs hardwired UART frames as valid:
  - `DANEVI_SONAR: Hardwired sonar frame len=4 bytes=FF 00 00 FF -> 0 mm`
- MAVLink timer now also publishes from the hardwired source using the parsed distance:
  - `Publishing hardwired sonar DISTANCE_SENSOR: 0 mm (0 cm)`

### Interpretation
- The protocol mismatch was real and is now fixed.
- The hardwired sonar path is now decoding the manufacturer frame format instead of rejecting valid packets.
- In the latest no-water / shallow-air runtime capture, the sensor repeatedly reported `0 mm`, which is consistent with the valid frame `FF 00 00 FF`.
- The earlier bucket captures showing frames like `FF 00 D5 D4` remain important evidence that, with the sensor in water, the parser should now be able to surface plausible depths around `209-214 mm`.

