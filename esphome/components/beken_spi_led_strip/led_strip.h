#pragma once

#ifdef USE_BK72XX

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/channel_colors.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::beken_spi_led_strip {

class BekenSPILEDStripLightOutput final : public light::AddressableLight {
 public:
  BekenSPILEDStripLightOutput(size_t num_leds, light::ChannelColors channel_colors)
      : buffer_(num_leds, {.channel_colors = channel_colors}, &this->correction_) {}

  light::ESPColorBuffer &buffer() override { return this->buffer_; }

  void setup() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override;

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    if (this->buffer_.layout().channel_colors.has_white()) {
      traits.set_supported_color_modes({light::ColorMode::RGB_WHITE, light::ColorMode::WHITE});
    } else {
      traits.set_supported_color_modes({light::ColorMode::RGB});
    }
    return traits;
  }

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_num_leds(uint16_t num_leds) { this->num_leds_ = num_leds; }
  void set_channel_colors(light::ChannelColors channel_colors) { this->channel_colors_ = channel_colors; }

  /// Set a maximum refresh rate in µs as some lights do not like being updated too often.
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

  void set_led_params(uint8_t bit0, uint8_t bit1, uint32_t spi_frequency);

  void dump_config() override;

 protected:
  light::InterleavedColorBuffer buffer_;

  uint8_t *dma_buf_{nullptr};

  uint8_t pin_;

  uint32_t spi_frequency_{6666666};
  uint8_t bit0_{0xE0};
  uint8_t bit1_{0xFC};

  uint32_t last_refresh_{0};
  optional<uint32_t> max_refresh_rate_{};
};

}  // namespace esphome::beken_spi_led_strip

#endif  // USE_BK72XX
