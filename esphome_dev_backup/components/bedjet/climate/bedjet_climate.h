#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/components/bedjet/bedjet_child.h"
#include "esphome/components/bedjet/bedjet_codec.h"
#include "esphome/components/bedjet/bedjet_hub.h"
#include "esphome/components/climate/climate.h"

#ifdef USE_ESP32

namespace esphome {
namespace bedjet {

class BedJetClimate : public climate::Climate, public BedJetClient, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  /* BedJetClient status update */
  void on_status(const BedjetStatusPacket *data) override;
  void on_bedjet_state(bool is_ready) override;
  std::string describe() override;

  /** Sets the default strategy to use for climate::CLIMATE_MODE_HEAT. */
  void set_heating_mode(BedjetHeatMode mode) { this->heating_mode_ = mode; }
  /** Sets the temperature source to use for the climate entity's current temperature */
  void set_temperature_source(BedjetTemperatureSource source) { this->temperature_source_ = source; }

  climate::ClimateTraits traits() override {
    auto traits = climate::ClimateTraits();
    traits.set_supports_action(true);
    traits.set_supports_current_temperature(true);
    traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_HEAT,
        // climate::CLIMATE_MODE_TURBO // Not supported by Climate: see presets instead
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_DRY,
    });

    // It would be better if we had a slider for the fan modes.
    traits.set_supported_custom_fan_modes(BEDJET_FAN_STEP_NAMES_SET);
    traits.set_supported_presets({
        // If we support NONE, then have to decide what happens if the user switches to it (turn off?)
        // climate::CLIMATE_PRESET_NONE,
        // Climate doesn't have a "TURBO" mode, but we can use the BOOST preset instead.
        climate::CLIMATE_PRESET_BOOST,
    });
    traits.set_supported_custom_presets({
        // We could fetch biodata from bedjet and set these names that way.
        // But then we have to invert the lookup in order to send the right preset.
        // For now, we can leave them as M1-3 to match the remote buttons.
        // EXT HT added to match remote button.
        "EXT HT",
        "M1",
        "M2",
        "M3",
    });
    if (this->heating_mode_ == HEAT_MODE_EXTENDED) {
      traits.add_supported_custom_preset("LTD HT");
    } else {
      traits.add_supported_custom_preset("EXT HT");
    }
    traits.set_visual_min_temperature(19.0);
    traits.set_visual_max_temperature(43.0);
    traits.set_visual_temperature_step(1.0);
    return traits;
  }

 protected:
  void control(const climate::ClimateCall &call) override;

  BedjetHeatMode heating_mode_ = HEAT_MODE_HEAT;
  BedjetTemperatureSource temperature_source_ = TEMPERATURE_SOURCE_AMBIENT;

  void reset_state_();
  bool update_status_();

  bool is_valid_() {
    // FIXME: find a better way to check this?
    return !std::isnan(this->current_temperature) && !std::isnan(this->target_temperature) &&
           this->current_temperature > 1 && this->target_temperature > 1;
  }
};

}  // namespace bedjet
}  // namespace esphome

#endif
