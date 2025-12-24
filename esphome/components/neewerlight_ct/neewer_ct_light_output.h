#pragma once

#include "esphome/components/color_temperature/ct_light_output.h"
#include "esphome/components/ble_client/ble_client.h"
#include "neewer_ble_client.h"
#include "neewer_light_state.h"
#include "neewer_message.h"

#ifdef USE_ESP32

namespace esphome::neewerlight_ct {

class NeewerCTLightOutput : public Component,
                            public color_temperature::CTLightOutput,
                            public ble_client::BLEClientNode {
 public:
  explicit NeewerCTLightOutput();
  void dump_config() override;

  // BLEClientNode override
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

 protected:
  // CTLightOutput override
  void write_state(light::LightState *state) override;

  // Methods to prepare messages and send them via BLE
  void turn_on_off_(OnOffState on_off);
  void change_color_temperature_and_brightness_(int color_temperature, int brightness);
  void change_brightness_(int brightness_percent);

  // Translate Message to Neewer protocol
  static std::vector<uint8_t> neewer_msg(const Message &msg);

  NeewerLightState old_state_;
  NeewerBleClient ble_;
};

}  // namespace esphome::neewerlight_ct

#endif  // USE_ESP32
