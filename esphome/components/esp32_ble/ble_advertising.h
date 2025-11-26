#pragma once

#include "esphome/core/defines.h"

#include <array>
#include <functional>
#include <span>
#include <vector>

#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_ADVERTISING

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>

namespace esphome::esp32_ble {

using raw_adv_data_t = struct {
  uint8_t *data;
  size_t length;
  esp_power_level_t power_level;
};

class ESPBTUUID;

class BLEAdvertising {
 public:
  BLEAdvertising(uint32_t advertising_cycle_time);

  void loop();

  void add_service_uuid(ESPBTUUID uuid);
  void remove_service_uuid(ESPBTUUID uuid);
  void set_scan_response(bool scan_response) { this->scan_response_ = scan_response; }
  void set_min_preferred_interval(uint16_t interval) { this->advertising_data_.min_interval = interval; }
  void set_manufacturer_data(const std::vector<uint8_t> &data);
  void set_appearance(uint16_t appearance) { this->advertising_data_.appearance = appearance; }
  void set_service_data(const std::vector<uint8_t> &data);
  void set_service_data(std::span<const uint8_t> data);
  void set_include_name(bool include_name) { this->include_name_in_adv_ = include_name; }
  void register_raw_advertisement_callback(std::function<void(bool)> &&callback);

  void start();
  void stop();

 protected:
  esp_err_t services_advertisement_();

  bool scan_response_;
  bool include_name_in_adv_{false};
  esp_ble_adv_data_t advertising_data_;
  esp_ble_adv_data_t scan_response_data_;
  esp_ble_adv_params_t advertising_params_;
  std::vector<ESPBTUUID> advertising_uuids_;

  std::vector<std::function<void(bool)>> raw_advertisements_callbacks_;

  const uint32_t advertising_cycle_time_;
  uint32_t last_advertisement_time_{0};
  int8_t current_adv_index_{-1};  // -1 means standard scan response
};

}  // namespace esphome::esp32_ble

#endif  // USE_ESP32_BLE_ADVERTISING
#endif  // USE_ESP32
