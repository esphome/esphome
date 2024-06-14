#pragma once

#include <vector>
#include <array>

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>

namespace esphome {
namespace esp32_ble {

// NOLINTNEXTLINE(modernize-use-using)
typedef struct {
  uint8_t flags[3];
  uint8_t length;
  uint8_t type;
  uint8_t company_id[2];
  uint8_t beacon_type[2];
} __attribute__((packed)) esp_ble_ibeacon_head_t;

// NOLINTNEXTLINE(modernize-use-using)
typedef struct {
  uint8_t proximity_uuid[16];
  uint16_t major;
  uint16_t minor;
  uint8_t measured_power;
} __attribute__((packed)) esp_ble_ibeacon_vendor_t;

// NOLINTNEXTLINE(modernize-use-using)
typedef struct {
  esp_ble_ibeacon_head_t ibeacon_head;
  esp_ble_ibeacon_vendor_t ibeacon_vendor;
} __attribute__((packed)) esp_ble_ibeacon_t;

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
  void set_ibeacon_data(std::array<uint8_t, 16> uuid, uint16_t major, uint16_t minor, int8_t measured_power);

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
