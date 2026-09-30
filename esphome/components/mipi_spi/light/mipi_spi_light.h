#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/mipi_spi/mipi_spi.h"
#include "esphome/core/helpers.h"

namespace esphome::mipi_spi {

// Brightness-only light that sets the display brightness with an SPI command.
// Brightness is scaled into [min_brightness, max_brightness]; off writes min_brightness.
class MipiSpiLight final : public light::LightOutput, public Parented<MipiSpiBrightness> {
 public:
  MipiSpiLight(uint8_t min_brightness, uint8_t max_brightness)
      : min_brightness_(min_brightness), max_brightness_(max_brightness) {}

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
    return traits;
  }

  void write_state(light::LightState *state) override {
    float brightness;
    state->current_values_as_brightness(&brightness);
    this->parent_->set_brightness(
        this->min_brightness_ +
        static_cast<uint8_t>(roundf(brightness * (this->max_brightness_ - this->min_brightness_))));
  }

 protected:
  uint8_t min_brightness_;
  uint8_t max_brightness_;
};

}  // namespace esphome::mipi_spi
