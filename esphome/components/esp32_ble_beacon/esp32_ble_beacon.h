#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble/ble.h"

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_bt.h>

namespace esphome {
namespace esp32_ble_beacon {

using namespace esp32_ble;

class ESP32BLEBeacon : public Component, public GAPEventHandler, public Parented<ESP32BLE> {
 public:
  explicit ESP32BLEBeacon(const std::array<uint8_t, 16> &uuid) : uuid_(uuid) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_major(uint16_t major) { this->major_ = major; }
  void set_minor(uint16_t minor) { this->minor_ = minor; }
  void set_min_interval(uint16_t val) { this->min_interval_ = val; }
  void set_max_interval(uint16_t val) { this->max_interval_ = val; }
  void set_measured_power(int8_t val) { this->measured_power_ = val; }
  void set_tx_power(int8_t val) { this->tx_power_ = val; }
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

 protected:
  std::array<uint8_t, 16> uuid_;
  uint16_t major_{};
  uint16_t minor_{};
  uint16_t min_interval_{};
  uint16_t max_interval_{};
  int8_t measured_power_{};
  int8_t tx_power_{};
  bool finished_init_{false};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ESP32BLEBeacon *global_esp32_ble_beacon;

}  // namespace esp32_ble_beacon
}  // namespace esphome

#endif
