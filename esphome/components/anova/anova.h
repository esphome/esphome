#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/climate/climate.h"
#include "anova_base.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp_gattc_api.h>

namespace esphome {
namespace anova {

namespace espbt = esphome::esp32_ble_tracker;

static const uint16_t ANOVA_SERVICE_UUID = 0xFFE0;
static const uint16_t ANOVA_CHARACTERISTIC_UUID = 0xFFE1;

class Anova : public climate::Climate, public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  climate::ClimateTraits traits() {
    auto traits = climate::ClimateTraits();
    traits.set_supports_current_temperature(true);
    traits.set_supports_heat_mode(true);
    traits.set_visual_min_temperature(25.0);
    traits.set_visual_max_temperature(100.0);
    traits.set_visual_temperature_step(0.1);
    return traits;
  }

 protected:
  AnovaCodec *codec_;
  void control(const climate::ClimateCall &call) override;
  uint16_t char_handle_;
  uint8_t current_request_;
};

}  // namespace anova
}  // namespace esphome

#endif
