# Finish To-Do

Current remaining items to close out before calling the project stable:

1. Fix the frontend fetch abort noise in `firmware/frontend/dronebridge.js`.
2. Reconcile AP fallback/IP behavior so the configured AP IP and the runtime fallback IP are consistent.
3. Run a longer soak test with the web UI open and both sonar modes exercised.
4. Validate end-to-end delivery to a real flight controller for both sonar sources.

Notes:
- The major crashers found on `11-03-2026` were addressed:
  - `httpd` stack overflow
  - `Tmr Svc` stack overflow
  - hardwired UART parser mismatch
  - settings save/reboot crash chain
  - main-task stack regression introduced during save-path hardening
- Archived probe/build logs and backups are kept locally under `archive/`.
