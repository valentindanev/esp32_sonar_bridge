# ESP32 OTA Migration Plan

Date: `04-04-2026`

## Goal

Reduce bait-boat maintenance pain by making the ESP32 updateable over Wi-Fi
after one final wired migration flash.

## Why a migration is required

The current firmware works, but it is not OTA-ready:

- the live app is built as a `2MB` image
- the hardware reports `4MB` of real flash
- the current layout is single-app only
- the web UI lives in a separate SPIFFS image

That means a safe OTA path needs:

1. a new partition table
2. rollback support
3. an app-level OTA endpoint
4. a recovery path that still works even if SPIFFS is damaged

## Chosen architecture

### 1. Flash layout

New partition table file:

- [partitions_ota_4mb.csv](/x:/backup/valentin/AI-Lab/projects/archive/esp32_sonar_bridge/firmware/partitions_ota_4mb.csv)

Layout:

- `nvs`
- `otadata`
- `phy_init`
- `ota_0` (`1536K`)
- `ota_1` (`1536K`)
- `www` (`384K`)

Rationale:

- the current app already fits comfortably inside `1536K`
- two OTA slots enable safe app updates and rollback
- SPIFFS stays separate so the existing web UI pipeline still works

### 2. Embedded recovery/update page

The main web UI remains in SPIFFS, but OTA is exposed through a dedicated
embedded page at:

- `/ota`

That page lives in the app binary itself, so it survives SPIFFS damage and
still gives the boat a recovery/update path.

### 3. Supported Wi-Fi update actions

The migration firmware adds:

- app OTA upload via `db_esp32.bin`
- SPIFFS web UI upload via `www.bin`
- a one-time “boot into AP update mode” flag for the next restart

### 4. Rollback behavior

Bootloader rollback is enabled.

The app confirms a newly booted OTA image only after the startup path completes.
That keeps rollback protection meaningful for bad early-boot updates.

## Expected update workflow after migration

### Normal firmware update

1. Power the boat.
2. Open `/ota`.
3. Upload `db_esp32.bin`.
4. Wait for automatic reboot.

### Web UI update

1. Open `/ota`.
2. Upload `www.bin`.
3. Wait for automatic reboot.

### Force a visible AP for maintenance

1. Open `/ota`.
2. Click `Reboot Into AP Update Mode`.
3. On the next boot, the ESP skips the Deeper boot probe and comes up in AP mode once.
4. Future boots return to normal behavior.

## One-time wired migration checklist

This still requires a normal serial flash one last time:

1. build the OTA migration firmware with the new `4MB` settings
2. flash bootloader + partition table + app + web UI over USB
3. verify `/ota` loads over Wi-Fi
4. verify a test OTA app upload works
5. verify a test `www.bin` upload works

## Safety notes

- If the SPIFFS upload is interrupted and the main UI breaks, `/ota` should
  still be available from the app binary after reboot.
- Bootloader or partition-table changes still require a wired flash.
- Ordinary app and web UI updates should no longer require opening the bait
  boat once the migration flash succeeds.
