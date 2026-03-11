# Finish To-Do

Current remaining items to close out before calling the project stable:

1. Fix the frontend fetch abort noise in `firmware/frontend/dronebridge.js`.
2. Suppress the Deeper STA reconnect attempt during intentional reboot so `ESP_ERR_WIFI_NOT_STARTED` no longer appears in the shutdown log.
3. Run a longer soak test with the web UI open and both sonar modes exercised.
4. Validate end-to-end delivery to a real flight controller for both sonar sources.

Notes:
- The major crashers found on `11-03-2026` were addressed:
  - `httpd` stack overflow
  - `Tmr Svc` stack overflow
  - hardwired UART parser mismatch
  - settings save/reboot crash chain
  - main-task stack regression introduced during save-path hardening
  - Deeper boot-session AP credential leak into the main Wi-Fi settings UI
- Known non-blocking quirk:
  - ESP-IDF still logs a temporary AP startup line for `192.168.4.1`, but the actual configured runtime AP remains `192.168.5.1`
- Archived probe/build logs and backups are kept locally under `archive/`.
