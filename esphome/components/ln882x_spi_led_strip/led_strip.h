#pragma once

#ifdef USE_LN882X

#include <cstddef>
#include <cstdint>

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/channel_colors.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::ln882x_spi_led_strip {

class LN882xSPILEDStripLightOutput final : public light::AddressableLight {
 public:
  /// The SPI encoding comes from light.py: SPI clock APB0 / 2^prescaler, the number of high SPI bits for a
  /// 0 and a 1 bit, and the zero bytes of the reset gap.
  LN882xSPILEDStripLightOutput(uint8_t pin, uint16_t num_leds, light::ChannelColors channel_colors, uint8_t prescaler,
                               uint8_t bit0_high_bits, uint8_t bit1_high_bits, uint16_t reset_bytes);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

  light::LightTraits get_traits() override { return this->channel_colors_.traits(); }
  int32_t size() const override { return this->num_leds_; }
  void clear_effect_data() override;
  void write_state(light::LightState *state) override;

  /// Set a maximum refresh rate in µs as some lights do not like being updated too often.
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
    return this->channel_colors_.view(this->color_data_, this->effect_data_, index, &this->correction_);
  }

  const size_t color_data_bytes_;
  const size_t dma_data_bytes_;
  uint8_t *color_data_{nullptr};
  uint8_t *effect_data_{nullptr};
  uint8_t *dma_data_{nullptr};
  uint32_t last_refresh_{0};
  uint32_t max_refresh_rate_{0};

  const light::ChannelColors channel_colors_;
  const uint16_t num_leds_;
  const uint8_t pin_;
  const uint8_t prescaler_;
  const uint8_t bit0_high_bits_;
  const uint8_t bit1_high_bits_;
};

}  // namespace esphome::ln882x_spi_led_strip

#endif  // USE_LN882X
