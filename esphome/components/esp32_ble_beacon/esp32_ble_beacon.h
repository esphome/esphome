#pragma once

#include "esphome/core/component.h"

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

class ESP32BLEBeacon : public Component {
 public:
  explicit ESP32BLEBeacon(const std::array<uint8_t, 16> &uuid) : uuid_(uuid) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_major(uint16_t major) { this->major_ = major; }
  void set_minor(uint16_t minor) { this->minor_ = minor; }
  void set_min_interval(uint16_t val) { this->min_interval_ = val; }
  void set_max_interval(uint16_t val) { this->max_interval_ = val; }
  void set_measured_power(int8_t val) { this->measured_power_ = val; }
  void set_tx_power(int8_t val) { this->tx_power_ = val; }

 protected:
  static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  static void ble_core_task(void *params);
  static void ble_setup();

  std::array<uint8_t, 16> uuid_;
  uint16_t major_{};
  uint16_t minor_{};
  uint16_t min_interval_{};
  uint16_t max_interval_{};
  int8_t measured_power_{};
  int8_t tx_power_{};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ESP32BLEBeacon *global_esp32_ble_beacon;

}  // namespace esp32_ble_beacon
}  // namespace esphome

#endif
