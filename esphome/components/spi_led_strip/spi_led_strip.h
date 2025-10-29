#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace spi_led_strip {

static const char *const TAG = "spi_led_strip";
class SpiLedStrip : public light::AddressableLight,
                    public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                          spi::DATA_RATE_1MHZ> {
 public:
  SpiLedStrip(uint16_t num_leds);
  void setup() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  int32_t size() const override { return this->num_leds_; }

  light::LightTraits get_traits() override;

  void dump_config() override;

  void write_state(light::LightState *state) override;

  void clear_effect_data() override { memset(this->effect_data_, 0, this->num_leds_ * sizeof(this->effect_data_[0])); }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  size_t buffer_size_{};
  uint8_t *effect_data_{nullptr};
  uint8_t *buf_{nullptr};
  uint16_t num_leds_;
};

}  // namespace spi_led_strip
}  // namespace esphome
