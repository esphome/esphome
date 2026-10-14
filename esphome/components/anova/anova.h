#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/climate/climate.h"
#include "anova_base.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome::anova {

namespace espbt = esphome::esp32_ble_tracker;

static const uint16_t ANOVA_SERVICE_UUID = 0xFFE0;
static const uint16_t ANOVA_CHARACTERISTIC_UUID = 0xFFE1;

class Anova final : public climate::Climate, public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void dump_config() override;
  climate::ClimateTraits traits() override {
    auto traits = climate::ClimateTraits();
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::ClimateMode::CLIMATE_MODE_HEAT});
    traits.set_visual_min_temperature(25.0);
    traits.set_visual_max_temperature(100.0);
    traits.set_visual_temperature_step(0.1);
    return traits;
  }
  void set_unit_of_measurement(const char *unit);

 protected:
  // A poll cycle re-asserts the configured unit, then reads device state.
  // Re-asserting every cycle prevents the cooker from silently reverting to
  // its default (Celsius); previously the unit was only set once on
  // connection, so a drift persisted (and corrupted the F/C interpretation of
  // subsequent readings) until the BLE link was re-established.
  enum class PollStep : uint8_t { SET_UNIT, STATUS, TARGET, CURRENT, IDLE };

  void write_request_(AnovaPacket *pkt);

  std::unique_ptr<AnovaCodec> codec_;
  void control(const climate::ClimateCall &call) override;
  uint16_t char_handle_;
  bool want_fahrenheit_{true};  // configured target unit; never overwritten by device replies
  PollStep poll_step_{PollStep::IDLE};
};

}  // namespace esphome::anova

#endif
