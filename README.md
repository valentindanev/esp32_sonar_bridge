# ESP32 Sonar & MAVLink Bridge

**Status**: In Development  
**Hardware**: ESP32 (replacing ESP8266 and Arduino Nano)  
**Stack**: C++ (Arduino IDE / PlatformIO)  

## Latest Debug Status
*Updated on March 11, 2026*

### Confirmed working now
- The ESP32 now connects to the Deeper AP, sends the official UDP enable request, and receives live NMEA directly from the sonar.
- The web page now shows:
  - Deeper SSID
  - Deeper password
  - human-readable Deeper depth
  - water temperature
  - satellites / GPS fix
  - raw NMEA debug text box
- Live example values from the current desk test:
  - depth: `0.00 m`
  - water temperature: `31.4 C`
  - satellites: `0`
  - GPS fix: none
- MAVLink output is now verified:
  - a 5-second TCP capture from the ESP32 telemetry port showed `DISTANCE_SENSOR` (`msgid 132`) packets being emitted continuously
  - the same capture also contained `HEARTBEAT` and `RADIO_STATUS`
- Important limitation:
  - the current build uses UART serial mode on the ESP32, not USB/JTAG serial mode
  - so the USB port cannot be used as a fake FC link on this classic ESP32 board
  - radio-side TCP/UDP capture is the practical way to validate outgoing MAVLink without a flight controller

### Solved in this session
- The Wi-Fi STA credential corruption bug in the Deeper fallback path is fixed.
- Root cause 1: the code used `sizeof(DB_PARAM_WIFI_SSID)` and `sizeof(DB_PARAM_PASS)` on string-pointer macros, so only pointer-sized fragments were copied and old tail bytes remained.
- Root cause 2: the Wi-Fi client configuration path populated `wifi_config.ap.*` instead of `wifi_config.sta.*`.
- The STA failure to AP fallback crash is also fixed.
- Root cause 3: the STA cleanup path left Wi-Fi runtime state alive, so re-entering AP mode crashed in `esp_event_loop_create_default()`.
- After rebuilding and flashing, the ESP32 now:
  - logs clean STA credentials
  - retries Deeper STA mode 15 times
  - falls back cleanly into DroneBridge AP mode without crashing

### Confirmed runtime state
- The board now loads the saved NVS setting `ss_deeper_ssid: Deeper CHIRP+ 3B6D`.
- The current runtime log shows:
  - `Using open security for Deeper STA connection`
  - `Init of WiFi Client-Mode finished. (SSID: Deeper CHIRP+ 3B6D PASS: <open>)`
  - `wifi:connected with Deeper CHIRP+ 3B6D`
  - `IP_EVENT_STA_GOT_IP:192.168.10.2`
  - `Deeper Fallback SUCCESS! We are now connected to the Deeper Sonar AP.`
- The current working build/flash path on this machine is `C:\Users\valen\esp32_sonar_build`.
  - Building directly from the UNC or mapped-share copy is unreliable because ESP-IDF frontend steps resolve back to UNC paths on Windows.

### Latest bug now blocking progress
- The Deeper Wi-Fi connection path is now working.
- A Windows scan of the live sonar AP shows:
  - `SSID: Deeper CHIRP+ 3B6D`
  - `Authentication: Open`
  - `Encryption: None`
- The open-AP fix resolved the prior connect failures:
  - previous `210`: `WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY`
  - previous `201`: `WIFI_REASON_NO_AP_FOUND`
- The latest visible runtime issue is different:
  - repeated `ESP_ERR_WIFI_MODE` warnings from `./main/db_esp32_control.c` line `749`
  - failing call: `esp_wifi_ap_get_sta_list(&wifi_sta_list)`
- Practical effect: once running in STA mode on the Deeper AP, some code still assumes AP mode and repeatedly calls an AP-only ESP-IDF API.
- The source has now been patched to use the actual ESP-IDF Wi-Fi runtime mode for those decisions.
- Additional debug instrumentation was also added:
  - inbound TCP/UDP packet previews while running in STA mode
  - throttled hardwired sonar frame logs
  - periodic hardwired `DISTANCE_SENSOR` publish logs
- The first logging build exposed a new issue:
  - `***ERROR*** A stack overflow in task danevi_sonar has been detected.`
  - cause: the new hardwired-sonar logging exceeded the task's old 2048-byte stack
- This has now been fixed in source by increasing the `danevi_sonar` task stack to `4096`.
- The current source now contains the first open-AP fix attempt:
  - Deeper target uses `WIFI_AUTH_OPEN`
  - Deeper target clears the STA password
  - runtime logging prints `<open>` instead of a fake password for that path
- This patched firmware built successfully on March 10, 2026 in `C:\Users\valen\esp32_sonar_build`.
- This patched firmware was flashed successfully to `COM13` and verified on boot.
- A newer debug build with the runtime-mode fix and extra logging also built successfully on March 10, 2026.
- The latest follow-up build with the `danevi_sonar` stack fix also built successfully and is not flashed yet.
- That follow-up build has now been flashed successfully.
- The `danevi_sonar` stack overflow is gone on the flashed build.
- In the latest captured run:
  - no `danevi_sonar` reboot loop
  - no repeated `ESP_ERR_WIFI_MODE` warnings were observed
  - the ESP again failed to join Deeper and fell back cleanly with `reason: 201`
- No Deeper packet-preview logs appeared in that run because STA connection did not succeed.
- No hardwired-sonar frame logs appeared in that run either, which suggests no valid hardwired sonar frames were captured during that boot window.
- Hardware note for the next session:
  - the hardwired sonar is currently not connected, so live hardwired-sonar testing is blocked for now and must be revisited later.

## Architecture Overview & Decisions
*Documented on March 9, 2026*

This project upgrades the boat's telemetry and sonar systems by unifying two legacy microcontrollers (an ESP WiFi MAVLink bridge and an Arduino Nano hardwired sonar reader) into a single **ESP32** hub.

### The Problem
The boat has two sonars:
1. **Hardwired Sonar**: Used 99% of the time. (L04xMTW using custom NMEA Arduino logic)
2. **Deeper Sonar (WiFi)**: Used 1% of the time for depth mapping on lakes.

Previously, handling the Deeper Sonar required connecting to its WiFi AP, which meant the ESP couldn't host an AP for the phone/laptop to receive MAVLink telemetry without heavy network latency (Single Radio Bottleneck). Furthermore, parsing the Hardwired Sonar at 115200 baud bogged down the Arduino Nano.

### The Solution: DroneBridge ESP32 & FreeRTOS State Machine
We are using the [DroneBridge ESP32](https://github.com/DroneBridge/ESP32) project as our foundation because it is actively maintained and built natively for the dual-core ESP32.

#### 1. The Boot-Time State Machine (The mavesp8266 Donor Logic)
To prevent the ESP32's single 2.4GHz WiFi radio from bogging down by running as an Access Point (AP) and a Station (STA) simultaneously, we use a Boot-Time State Machine copied from a community-modified `mavesp8266` build.
- **Scan Phase (0-15 Seconds)**: The ESP32 scans for a specific WiFi SSID (e.g., `Deeper CHIRP+`).
- **Branch A (Lake Mapping Mode)**: If found, the ESP32 connects to it as a Station (`STA`), reads the Deeper TCP/UDP stream, translates the depth to MAVLink, and injects it into the DroneBridge MAVLink proxy loop.
  - Note: Our specific Deeper SSID is `Deeper CHIRP+ 3B6D`.
- **Branch B (Normal Bridge Mode)**: If 15 seconds pass and the Deeper is *not* found, it abandons the connection, spins up the default DroneBridge Access Point (`AP`), and reads solely from the Hardwired Sonar.

#### 2. Dual-Core Distribution
- **Core 0**: Handles the native DroneBridge networking (WiFi AP/STA connections, TCP/UDP clients, WebUI).
- **Core 1 (FreeRTOS Task)**: Handles the dedicated Hardwired Sonar UART initialization, pinging (`0x55`), NMEA parsing, and distance calculations entirely unblocked.

#### 3. Update & Maintainability Strategy
- **Easy to Build > Easy to Update**: We will directly inject our sonar logic into DroneBridge's `main.c` and `db_mavlink_msgs.c` files. This is slightly messy but ensures an incredibly stable and straightforward build process for a boat that will be physically sealed.
- **Over-The-Air (OTA)**: DroneBridge natively supports WebUI OTA flashing. If we ever need to update the code, we upload the new `.bin` through the browser at `192.168.2.1` without opening the boat.
- **Configurable WebUI**: The DroneBridge `index.html` config page will be permanently modified so the user can change the Sonar RX pin and the "Deeper SSID" from the web browser without recompiling the code.

## Historic Task Checklist
- [x] Review current Arduino Nano code for the hardwired sonar.
  - Note: Sonar L04xMTW. Code uses SoftwareSerial at 115200. On ESP32, this will be upgraded to HardwareSerial (UART2) for stability and pinned to Core 1.
  - Source: https://github.com/AlksSAV/Sonar-to-i2c-NMEA-/blob/main/sonar_to_nmea__SDDBT.ino
- [x] Review current ESP code for the MAVLink WiFi bridge.
  - Note: Based on DroneBridge ESP32. It natively supports MAVLink parsing to inject RSSI/Radio packets. This means we can hook into its existing parsing loop to inject our custom NMEA `$SDDBT` sonar strings before they get sent over WiFi or UART.
  - Source: https://github.com/DroneBridge/ESP32
- [x] Review alternative MAVLink bridge: `mavesp8266` (Original and Deeper-Modified version).
  - Note: Looked at the Deeper modified version. It uses a simple loop in `setup()` to attempt connection to STA mode for 15 seconds. If it fails, it falls back to AP mode. This proves the AP/STA fallback architecture is viable and actively used by others.
  - Source Original: https://github.com/BeyondRobotix/mavesp8266
  - Source Modified (Deeper Support): https://gitlab.com/somepublic/mavesp8266
- [x] Download donor repositories into `/donors` for offline access.
