#pragma once

#include <vector>

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>

namespace esphome {
namespace esp32_ble {

class ESPBTUUID;

class BLEAdvertising {
 public:
  BLEAdvertising();

  void add_service_uuid(ESPBTUUID uuid);
  void remove_service_uuid(ESPBTUUID uuid);
  void set_scan_response(bool scan_response) { this->scan_response_ = scan_response; }
  void set_min_preferred_interval(uint16_t interval) { this->advertising_data_.min_interval = interval; }
  void set_manufacturer_data(const std::vector<uint8_t> &data);
  void set_service_data(const std::vector<uint8_t> &data);

  void start();
  void stop();

 protected:
  bool scan_response_;
  esp_ble_adv_data_t advertising_data_;
  esp_ble_adv_data_t scan_response_data_;
  esp_ble_adv_params_t advertising_params_;
  std::vector<ESPBTUUID> advertising_uuids_;
};

}  // namespace esp32_ble
}  // namespace esphome

#endif
