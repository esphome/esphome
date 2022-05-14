#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_bt.h>

namespace esphome {
namespace esp32_ble_beacon {

// NOLINTNEXTLINE(modernize-use-using)
typedef struct {
  uint8_t flags[3];
  uint8_t length;
  uint8_t type;
  uint16_t company_id;
  uint16_t beacon_type;
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

class ESP32BLEBeacon : public Component, public esp32_ble_tracker::ESPBTClient {
 public:
  explicit ESP32BLEBeacon(const std::array<uint8_t, 16> &uuid) : uuid_(uuid) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_major(uint16_t major) { this->major_ = major; }
  void set_minor(uint16_t minor) { this->minor_ = minor; }
  void set_txpower_level(uint16_t level) { this->txpower_level_ = (esp_power_level_t) level; }
  void set_measured_power(int16_t power) { this->measured_power_ = power; }

 protected:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override { return false; };
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override{};
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void connect() override{};

  std::array<uint8_t, 16> uuid_;
  uint16_t major_{};
  uint16_t minor_{};
  esp_power_level_t txpower_level_{};
  int16_t measured_power_{};
};

}  // namespace esp32_ble_beacon
}  // namespace esphome

#endif
