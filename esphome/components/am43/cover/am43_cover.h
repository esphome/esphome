#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/am43/am43_base.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome {
namespace am43 {

namespace espbt = esphome::esp32_ble_tracker;

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
  void set_invert_position(bool invert_position) { this->invert_position_ = invert_position; }

 protected:
  void control(const cover::CoverCall &call) override;
  uint16_t char_handle_;
  uint16_t pin_;
  bool invert_position_;
  std::unique_ptr<Am43Encoder> encoder_;
  std::unique_ptr<Am43Decoder> decoder_;
  bool logged_in_;

  float position_;
};

}  // namespace am43
}  // namespace esphome

#endif
