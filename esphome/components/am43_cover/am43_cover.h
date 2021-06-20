#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/cover/cover.h"
#include "am43_base.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp_gattc_api.h>

namespace esphome {
namespace am43 {

namespace espbt = esphome::esp32_ble_tracker;

static const uint16_t AM43_SERVICE_UUID = 0xFE50;
static const uint16_t AM43_CHARACTERISTIC_UUID = 0xFE51;

// Tuya identifiers, only to detect and warn users as they are incompatible.
static const uint16_t AM43_TUYA_SERVICE_UUID = 0x1910;
static const uint16_t AM43_TUYA_CHARACTERISTIC_UUID = 0x2b11;

class Am43Component : public cover::Cover, public esphome::ble_client::BLEClientNode, public Component {
 public:
  void setup() override;
  void loop() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  cover::CoverTraits get_traits() override;
  void set_pin(uint16_t pin) { this->pin_ = pin; }

 protected:
  void control(const cover::CoverCall &call) override;
  uint16_t char_handle_;
  uint16_t pin_;
  Am43Encoder *encoder_;
  Am43Decoder *decoder_;
  bool logged_in_;

  float position_;
};

}  // namespace am43
}  // namespace esphome

#endif
