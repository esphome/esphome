#pragma once

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>

namespace esphome::esp32_ble {

// Structure for BLE scan results - only fields we actually use
struct __attribute__((packed)) BLEScanResult {
  esp_bd_addr_t bda;
  uint8_t ble_addr_type;
  int8_t rssi;
  uint8_t ble_adv[ESP_BLE_ADV_DATA_LEN_MAX + ESP_BLE_SCAN_RSP_DATA_LEN_MAX];
  uint8_t adv_data_len;
  uint8_t scan_rsp_len;
  uint8_t search_evt;
};  // ~73 bytes vs ~400 bytes for full esp_ble_gap_cb_param_t

}  // namespace esphome::esp32_ble

#endif
