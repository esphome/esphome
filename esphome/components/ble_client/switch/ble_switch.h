#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/switch/switch.h"

#ifdef USE_ESP32
#include <esp_gattc_api.h>

namespace esphome {
namespace ble_client {

namespace espbt = esphome::esp32_ble_tracker;

class BLEClientSwitch : public switch_::Switch, public Component, public BLEClientNode {
 public:
  void dump_config() override;
  void loop() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void write_state(bool state) override;
};

}  // namespace ble_client
}  // namespace esphome
#endif
