/*
 *   This file is part of DroneBridge: https://github.com/DroneBridge/ESP32
 *
 *   Copyright 2026 Wolfgang Christl
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

#include "db_timers.h"
#include "danevi_sonar.h"
#include "deeper_udp_sonar.h"
#include "db_mavlink_msgs.h"
#include "db_parameters.h"
#include "esp_log.h"
#include "globals.h"
#include <esp_wifi.h>


#define TAG "DB_TIMERS"

static bool db_wifi_runtime_has_sta(void) {
  wifi_mode_t mode = WIFI_MODE_NULL;
  return esp_wifi_get_mode(&mode) == ESP_OK &&
         (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA);
}

static bool db_wifi_runtime_has_ap(void) {
  wifi_mode_t mode = WIFI_MODE_NULL;
  return esp_wifi_get_mode(&mode) == ESP_OK &&
         (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA);
}

/**
 * Callback that reads the RSSI that the ESP32 is having to the AP it is
 * connected to
 * @param pxTimer
 */
void db_timer_wifi_rssi_callback(TimerHandle_t pxTimer) {
  // This function is called periodically by the FreeRTOS timer
  if (db_wifi_runtime_has_sta()) {
    // update rssi variable - set to -127 when not available
    if (esp_wifi_sta_get_rssi((int *)&db_esp_signal_quality.air_rssi) !=
        ESP_OK) {
      db_esp_signal_quality.air_rssi = -127;
      ESP_LOGE(TAG, "Failed to get RSSI");
    } else { /* all good */
    }
  } else if (db_wifi_runtime_has_ap() &&
             (DB_PARAM_RADIO_MODE == DB_WIFI_MODE_AP ||
              DB_PARAM_RADIO_MODE == DB_WIFI_MODE_AP_LR)) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_ap_get_sta_list(
        &wifi_sta_list)); // update list of connected stations
  }
}

/**
 * Reads the RSSI that the ESP32 is having to the AP it is connected to every
 * DB_TIMER_RSSI_PERIOD_MS Or updated the client list in AP mode to get the
 * latest RSSI of every client
 */
void db_timer_start_wifi_rssi_timer() {
  static TimerHandle_t xRssiTimer;
  xRssiTimer =
      xTimerCreate("RSSI_Timer", pdMS_TO_TICKS(DB_TIMER_RSSI_PERIOD_MS), pdTRUE,
                   (void *)0, db_timer_wifi_rssi_callback);

  if (xRssiTimer == NULL) {
    ESP_LOGE(TAG, "Failed to create RSSI timer.");
  }
  if (xRssiTimer != NULL) {
    ESP_LOGI(TAG, "Starting RSSI timer.");
    xTimerStart(xRssiTimer, 0);
  }
}

/**
 * Injects a MAVLink heartbeat if MAVLink is selected as serial telemetry
 * protocol. If necessary, a fake heartbeat for the FC as well.
 * @param pxTimer
 */
void db_timer_mavlink_heartbeat_callback(TimerHandle_t pxTimer) {
  static uint8_t buff[296];
  if (DB_PARAM_SERIAL_PROTO != DB_SERIAL_PROTOCOL_MAVLINK) {
    return; // Do not send heartbeat in non mavlink modes
  }
  if (db_get_mav_sys_id() != 0) {
    if (DB_PARAM_RADIO_MODE == DB_WIFI_MODE_ESPNOW_GND ||
        DB_PARAM_RADIO_MODE == DB_WIFI_MODE_AP_LR) {
      // In AP LR mode and in ESP-NOW GND mode the heartbeat has to be emitted
      // via serial directly to the GCS
      uint16_t length = db_mav_create_heartbeat(buff, &fmav_status_serial);
      write_to_serial(buff, length);
    } else {
      // Send heartbeat via radio interface
      uint16_t length = db_mav_create_heartbeat(buff, &fmav_status_radio);
      db_send_to_all_radio_clients(buff, length);
    }
  } else {
    // haven't seen a system ID from the FC yet so do not send any heartbeat or
    // heartbeats disabled
  }
}

/**
 * Starts a periodic MAVLink heartbeat sending timer.
 */
void db_timer_start_mavlink_heartbeat() {
  static TimerHandle_t xHeartBeatTimerHandle;
  xHeartBeatTimerHandle = xTimerCreate(
      "MAV_Heartbeat_Timer", pdMS_TO_TICKS(DB_TIMER_MAVLINK_HEARTBEAT_MS),
      pdTRUE, (void *)0, db_timer_mavlink_heartbeat_callback);

  if (xHeartBeatTimerHandle == NULL) {
    ESP_LOGE(TAG, "Failed to create heartbeat timer.");
  }
  if (xHeartBeatTimerHandle != NULL) {
    ESP_LOGI(TAG, "Starting to send heartbeats.");
    xTimerStart(xHeartBeatTimerHandle, 0);
  }
}

/**
 * Timer callback to send a Radio Status Message to the GND in case MAVLink is
 * set as serial protocol
 * @param pxTimer
 */
void db_timer_mavlink_radiostatus_callback(TimerHandle_t pxTimer) {
  if (DB_PARAM_SERIAL_PROTO != DB_SERIAL_PROTOCOL_MAVLINK) {
    return; // Do not send heartbeat in transparent mode
  }
  static uint8_t buff[296];
  bool runtime_sta = db_wifi_runtime_has_sta();
  bool runtime_ap = db_wifi_runtime_has_ap();
  // ESP32s that are connected to a flight controller via UART will send
  // RADIO_STATUS messages to the GND
  if (runtime_sta ||
      DB_PARAM_RADIO_MODE == DB_WIFI_MODE_ESPNOW_AIR ||
      DB_PARAM_RADIO_MODE == DB_BLUETOOTH_MODE) {
    // ToDo: For BLE only the last connected client is considered.
    fmav_radio_status_t payload_r = {
        .fixed = 0,
        .txbuf = 0,
        .noise = db_esp_signal_quality.gnd_noise_floor,
        .remnoise = db_esp_signal_quality.air_noise_floor,
        .remrssi = db_format_rssi(db_esp_signal_quality.air_rssi,
                                  db_esp_signal_quality.air_noise_floor),
        .rssi = db_format_rssi(db_esp_signal_quality.gnd_rssi,
                               db_esp_signal_quality.gnd_noise_floor),
        .rxerrors = db_esp_signal_quality.gnd_rx_packets_lost};
    uint16_t len = fmav_msg_radio_status_encode_to_frame_buf(
        buff, db_get_mav_sys_id(), db_get_mav_comp_id(), &payload_r,
        &fmav_status_radio);
    db_send_to_all_radio_clients(buff, len);
  } else if (runtime_ap && DB_PARAM_RADIO_MODE == DB_WIFI_MODE_AP &&
             wifi_sta_list.num > 0) {
    // We assume ESP32 is not used in DB_WIFI_MODE_AP on the ground but only on
    // the drone side
    // -> We are in WiFi AP mode and connected to the drone
    // Send each connected client a radio status packet.
    // ToDo: Only the RSSI of the first (Wi-Fi) is considered.
    //  Easier for UDP since we have a nice list with mac addresses to use for
    //  mapping. Harder for TCP -> no MAC addresses available of connected
    //  clients
    fmav_radio_status_t payload_r = {
        .fixed = UINT8_MAX,
        .noise = UINT8_MAX,
        .remnoise = UINT8_MAX,
        .remrssi = UINT8_MAX,
        .rssi = db_format_rssi(wifi_sta_list.sta[0].rssi, -88),
        .rxerrors = 0,
        .txbuf = 0};
    uint16_t len = fmav_msg_radio_status_encode_to_frame_buf(
        buff, db_get_mav_sys_id(), db_get_mav_comp_id(), &payload_r,
        &fmav_status_radio);
    db_send_to_all_radio_clients(buff, len);
  } else {
    // In AP LR or ESPNOW GND mode the clients will send the info to the GCS
    // directly, no need for the GND ESP32 to do anything
  }
}

/**
 * Starts a periodic MAVLink radio status sending timer.
 */
void db_timer_start_mavlink_radio_status() {
  static TimerHandle_t xRadioStatusTimerHandle;
  ;
  xRadioStatusTimerHandle = xTimerCreate(
      "MAV_Radiostatus_Timer", pdMS_TO_TICKS(DB_TIMER_MAVLINK_RADIOSTATUS_MS),
      pdTRUE, (void *)0, db_timer_mavlink_radiostatus_callback);

  if (xRadioStatusTimerHandle == NULL) {
    ESP_LOGE(TAG, "Failed to create Radio Status timer.");
  }
  if (xRadioStatusTimerHandle != NULL) {
    ESP_LOGI(TAG, "Starting to send radio status packets.");
    xTimerStart(xRadioStatusTimerHandle, 0);
  }
}

/**
 * Timer callback to send a DISTANCE_SENSOR MAVLink message based on the active
 * sonar source.
 * @param pxTimer
 */
void db_timer_mavlink_sonar_callback(TimerHandle_t pxTimer) {
  if (DB_PARAM_SERIAL_PROTO != DB_SERIAL_PROTOCOL_MAVLINK) {
    return; // Only inject in MAVLink mode
  }

  int distance_mm = -1;
  bool use_deeper_sonar = deeper_udp_sonar_get_latest_distance(&distance_mm);
  if ((use_deeper_sonar || danevi_sonar_get_latest_distance(&distance_mm)) &&
      distance_mm >= 0) {
    static uint8_t buff[296];
    static TickType_t last_sonar_log_tick = 0;

    fmav_distance_sensor_t payload = {0};
    payload.time_boot_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    payload.min_distance = use_deeper_sonar ? 0 : 28;
    payload.max_distance = use_deeper_sonar ? 10000 : 450;
    payload.current_distance = distance_mm / 10; // Convert mm to cm
    payload.type = 1; // MAV_DISTANCE_SENSOR_ULTRASOUND
    payload.id = use_deeper_sonar ? 1 : 0;
    payload.orientation = 25; // MAV_SENSOR_ROTATION_PITCH_270 (downward facing)
    payload.covariance = 0;

    uint16_t len = fmav_msg_distance_sensor_encode_to_frame_buf(
        buff, db_get_mav_sys_id() == 0 ? 1 : db_get_mav_sys_id(),
        191, // MAV_COMP_ID_GIMBAL or MAV_COMP_ID_PERIPHERAL
        &payload, &fmav_status_serial);

    // Send to Flight Controller (Serial) AND Ground Control Station (Radio)
    write_to_serial(buff, len);
    db_send_to_all_radio_clients(buff, len);

    TickType_t now = xTaskGetTickCount();
    if ((now - last_sonar_log_tick) >= pdMS_TO_TICKS(1000)) {
      ESP_LOGI(TAG, "Publishing %s sonar DISTANCE_SENSOR: %d mm (%d cm)",
               use_deeper_sonar ? "Deeper" : "hardwired", distance_mm,
               payload.current_distance);
      last_sonar_log_tick = now;
    }
  }
}

/**
 * Starts a periodic MAVLink distance sensor sending timer at 10Hz.
 */
void db_timer_start_mavlink_sonar() {
  static TimerHandle_t xSonarTimerHandle = NULL;
  if (xSonarTimerHandle == NULL) {
    xSonarTimerHandle =
        xTimerCreate("MAV_Sonar_Timer",
                     pdMS_TO_TICKS(DB_TIMER_MAVLINK_SONAR_MS), // 100ms
                     pdTRUE, (void *)0, db_timer_mavlink_sonar_callback);
    if (xSonarTimerHandle != NULL) {
      ESP_LOGI(TAG, "Starting to send DISTANCE_SENSOR packets.");
      xTimerStart(xSonarTimerHandle, 0);
    } else {
      ESP_LOGE(TAG, "Failed to create Sonar Timer!");
    }
  }
}
