#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/spi/spi.h"

namespace esphome::spi_led_strip {

ESPHOME_LOG_TAG(TAG, "spi_led_strip");
class SpiLedStrip final : public light::AddressableLight,
                          public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                                spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  SpiLedStrip(uint16_t num_leds);
  void setup() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  int32_t size() const override { return this->num_leds_; }

  light::LightTraits get_traits() override;

  void dump_config() override;

  void write_state(light::LightState *state) override;

  void clear_effect_data() override { this->internal_accessor_.clear_effect_data(this->num_leds_); }

 protected:
  light::CorrectedBufferColorAccessor internal_accessor_{
      nullptr, nullptr, {2, 1, 0, light::ChannelColors::RESERVED_WHITE}, &this->correction_};
  size_t buffer_size_{};
  uint16_t num_leds_;
};

}  // namespace esphome::spi_led_strip
