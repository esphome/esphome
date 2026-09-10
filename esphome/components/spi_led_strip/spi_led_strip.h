#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/spi/spi.h"

namespace esphome::spi_led_strip {

static const char *const TAG = "spi_led_strip";
class SpiLedStrip final : public light::AddressableLight,
                          public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                                spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  SpiLedStrip(size_t num_leds, light::ChannelColors channel_colors)
      : buffer_(num_leds,
                {.channel_colors = {.r = 3, .g = 2, .b = 1, .w = light::ChannelColors::NO_WHITE},
                 .bytes_per_led = 4,
                 .leading_bytes = 4,
                 .trailing_bytes = 4},
                &this->correction_) {}

  light::ESPColorBuffer &buffer() override { return this->buffer_; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  light::LightTraits get_traits() override;

  void dump_config() override;

  void write_state(light::LightState *state) override;

 protected:
  light::InterleavedColorBuffer buffer_;
};

}  // namespace esphome::spi_led_strip
