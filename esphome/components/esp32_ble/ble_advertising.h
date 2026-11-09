#pragma once

#include "esphome/core/defines.h"
#include "ble_uuid.h"

#include <array>
#include <functional>
#include <span>
#include <vector>

#ifdef USE_ESP32
#ifdef USE_ESP32_BLE_ADVERTISING

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>

namespace esphome::esp32_ble {

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
  /// Gives `uuid` and the service data the advertising packet to themselves, so passive scanners see them; the other
  /// service UUIDs, manufacturer data and appearance move to the scan response. Applies on the next refresh.
  void set_priority_uuid(ESPBTUUID uuid) { this->priority_uuid_ = uuid; }
  void clear_priority_uuid() { this->priority_uuid_ = {}; }
  void register_raw_advertisement_callback(std::function<void(bool)> &&callback);

  void start();
  void stop();
  void on_config_complete();
  /// The stack was torn down and its pending completion events dropped.
  void reset_config_state();
  void cancel_deferred_restart() { this->restart_pending_ = false; }

 protected:
  esp_err_t services_advertisement_();
  bool config_pending_() const;

  bool scan_response_;
  bool include_name_in_adv_{false};
  ESPBTUUID priority_uuid_;  // unset when no service has priority
  esp_ble_adv_data_t advertising_data_;
  esp_ble_adv_data_t scan_response_data_;
  esp_ble_adv_params_t advertising_params_;
  std::vector<ESPBTUUID> advertising_uuids_;

  std::vector<std::function<void(bool)>> raw_advertisements_callbacks_;

  const uint32_t advertising_cycle_time_;
  uint32_t last_advertisement_time_{0};
  uint32_t config_sent_time_{0};
  int8_t current_adv_index_{-1};  // -1 means standard scan response
  // Bluedroid keeps the last advertising and scan response config in one static buffer and builds the packet from it
  // later on another task; a new config before that frees it under the builder. Count the configs in flight.
  uint8_t configs_pending_{0};
  bool restart_pending_{false};
};

}  // namespace esphome::esp32_ble

#endif  // USE_ESP32_BLE_ADVERTISING
#endif  // USE_ESP32
