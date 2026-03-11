# Current Debugging Context - ESP32 Sonar Bridge

## Latest Session Update
- Date: 11-03-2026
- Compaction snapshot for this session:
  - public GitHub repo created: `https://github.com/valentindanev/esp32_sonar_bridge`
  - earlier nested Git metadata was preserved outside the project before publishing:
    - `X:\backup\valentin\AI-Lab\projects\esp32_sonar_bridge_firmware_git_BACKUP_20260311`
    - `X:\backup\valentin\AI-Lab\projects\esp32_sonar_bridge_nested_git_BACKUP_20260311`
- Source/UI work completed in this session:
  - Deeper readout boxes made smaller
  - GPS coordinates added to Deeper stats and frontend
  - hardwired depth/debug API and frontend panel added
  - `Enable Hardwired Sonar` toggle added
  - active sonar source now exposed in `/api/system/stats`
- Required boot policy from Valentin is now implemented in source, documented in `README.md`, built, and flashed successfully to `COM13`:
  1. On boot, if Deeper fallback is enabled, try Deeper exactly once for `60 seconds`
  2. If Deeper connects in that window: keep hardwired sonar off and use Deeper only for MAVLink `DISTANCE_SENSOR` until next reboot
  3. If Deeper does not connect in that window: fall back to AP mode once, stop trying Deeper until next reboot, start hardwired sonar, and use hardwired only for MAVLink `DISTANCE_SENSOR`
  4. Sonar source selection is fixed for the current boot
- Latest verified build/flash:
  - local build path: `C:\Users\valen\esp32_sonar_build`
  - latest app size: `0x11dae0`
  - flash to classic ESP32 on `COM13` succeeded on `11-03-2026`
- Latest verified runtime after the new boot-policy flash:
  - repeated logs now explicitly say `Retry to connect to the AP (...) within boot window (60000 ms)`
  - after `60000 ms` the ESP logs:
    - `Timed out after 60000 ms while trying to connect to SSID: Deeper CHIRP+ 3B6D`
    - `STA Mode failed (Deeper not found). Falling back to normal AP Mode.`
    - `Deeper boot probe failed. Staying in AP mode and using hardwired sonar until the next reboot.`
  - after that fallback the AP starts on `192.168.4.1`
  - after that fallback the hardwired sonar task starts and no more Deeper retries occur until reboot
- Important current interpretation:
  - the AP disappearing for roughly the first `60 seconds` after boot is now expected whenever Deeper fallback is enabled and the ESP is still inside the single boot-time Deeper probe window
  - this is not a reboot loop
- Current known open items:
  - Deeper still often fails to join during desk boots with `reason: 201` when the sonar is not definitely awake/advertising in water mode
  - hardwired sonar live data is still unverified because the physical sensor was not yet wired/tested after the new boot-policy flash
  - when no hardwired sensor is connected, repeated `DANEVI_SONAR: No hardwired sonar response within 100 ms` warnings are expected
  - the frontend fetch noise (`signal is aborted without reason`) was analyzed earlier and is likely caused by overlapping polling plus an uncleared `AbortController` timeout in `frontend/dronebridge.js`; this is not fixed yet
  - hardwired toggle semantics are now subordinate to the required boot policy:
    - if Deeper boot probe succeeds, hardwired stays off for that boot
    - if Deeper boot probe fails, hardwired is intentionally started for that boot even if the saved hardwired toggle is off
- Recommended next live steps:
  1. Reboot with Deeper absent and wait more than `60 seconds`; confirm the AP returns and stays stable
  2. Wire the hardwired sonar and validate depth/debug plus MAVLink `DISTANCE_SENSOR` in the AP fallback path
  3. Reboot with Deeper awake in water and confirm successful Deeper selection within the `60 second` window

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
  - file: `X:\backup\valentin\AI-Lab\projects\esp32_sonar_bridge\mavesp-esp12e-1.2.3.bin`
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
  - `X:\backup\valentin\AI-Lab\projects\esp32_sonar_bridge`
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
  - `X:\backup\valentin\AI-Lab\projects\esp32_sonar_bridge\firmware\main\db_esp32_control.c`
  - `X:\backup\valentin\AI-Lab\projects\esp32_sonar_bridge\firmware\main\db_timers.c`
  - `X:\backup\valentin\AI-Lab\projects\esp32_sonar_bridge\firmware\main\danevi_sonar.c`
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
- `X:\backup\valentin\AI-Lab\projects\esp32_sonar_bridge\firmware\main\db_esp32_control.c`
- `X:\backup\valentin\AI-Lab\projects\esp32_sonar_bridge\firmware\main\db_timers.c`
- `X:\backup\valentin\AI-Lab\projects\esp32_sonar_bridge\firmware\main\danevi_sonar.c`
- Mirrored build copy:
  - `C:\Users\valen\esp32_sonar_build\main\...`

## Next Steps for the Next Session
1. Re-test with the Deeper sonar definitely awake and advertising in water-mode.
2. If Deeper connects again, watch for the new TCP/UDP packet-preview logs to identify its traffic path and payload format.
3. Verify whether the previous `ESP_ERR_WIFI_MODE` warning is truly resolved during a successful STA session.
4. Check the hardwired sonar wiring / activity if no `DANEVI_SONAR` frame logs appear during a debug window.
