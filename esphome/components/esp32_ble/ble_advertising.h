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

namespace esphome::esp32_ble {

class BLEAdvertising {
 public:
  BLEAdvertising(uint32_t advertising_cycle_time);

  void loop();

  void add_service_uuid(ESPBTUUID uuid);
  void remove_service_uuid(ESPBTUUID uuid);
  void set_manufacturer_data(const std::vector<uint8_t> &data);
  void set_appearance(uint16_t appearance) { this->appearance_ = appearance; }
  void set_service_data(const std::vector<uint8_t> &data);
  void set_service_data(std::span<const uint8_t> data);
  void set_include_name(bool include_name) { this->include_name_in_adv_ = include_name; }
  /// Give `uuid` and up to 8 bytes of service data the advertising packet; the rest moves to the scan response.
  void set_priority_uuid(ESPBTUUID uuid) { this->priority_uuid_ = uuid; }
  void clear_priority_uuid() { this->priority_uuid_ = {}; }
  void register_raw_advertisement_callback(std::function<void(bool)> &&callback);

  void start();
  void stop();
  /// Send the services advertising from the next loop(); loop() is the only sender.
  void request_start() { this->restart_pending_ = true; }
  void cancel_deferred_restart() { this->restart_pending_ = false; }

 protected:
  // An element longer than this never fits in a 31 byte packet
  static constexpr size_t MAX_DATA_LEN = 29;

  esp_err_t services_advertisement_();

  ESPBTUUID priority_uuid_;  // unset when no service has priority
  esp_ble_adv_params_t advertising_params_;
  std::vector<ESPBTUUID> advertising_uuids_;

  std::vector<std::function<void(bool)>> raw_advertisements_callbacks_;

  const uint32_t advertising_cycle_time_;
  uint32_t last_advertisement_time_{0};
  std::array<uint8_t, MAX_DATA_LEN> manufacturer_data_;
  std::array<uint8_t, MAX_DATA_LEN> service_data_;
  uint16_t appearance_{0};
  int8_t current_adv_index_{-1};  // -1 means standard scan response
  uint8_t manufacturer_data_len_{0};
  uint8_t service_data_len_{0};
  bool include_name_in_adv_{false};
  bool restart_pending_{false};
};

}  // namespace esphome::esp32_ble

#endif  // USE_ESP32_BLE_ADVERTISING
#endif  // USE_ESP32
