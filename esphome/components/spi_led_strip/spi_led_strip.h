#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace spi_led_strip {

enum Protocol : uint8_t {
  DOTSTAR,  // <32 Bit 0s> <3 Bit 1s, 5 Bit brightness> <8 Bit Blue> <8 Bit Green> <8 Bit Red> ... <32 Bit 1s>
  RAW,      // <24 Bit Color> ...
};

static const char *const TAG = "spi_led_strip";
class SpiLedStrip : public light::AddressableLight,
                    public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                          spi::DATA_RATE_1MHZ> {
 public:
  SpiLedStrip(Protocol protocol, std::string channel_map, uint16_t num_leds);
  ~SpiLedStrip(void) = default;
  void setup(void) override;

  void set_min_mireds(float min_reds) { this->min_mireds_ = min_reds; }
  void set_max_mireds(float max_mireds) { this->max_mireds_ = max_mireds; }

  float get_setup_priority() const override { return setup_priority::IO; }

  int32_t size() const override { return this->num_leds_; }

  light::LightTraits get_traits() override;

  void dump_config() override;

  void write_state(light::LightState *state) override;

  void clear_effect_data() override { memset(this->effect_data_, 0, this->num_leds_ * sizeof(this->effect_data_[0])); }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  Protocol protocol_{};
  light::ChannelMap channel_map_{};
  uint16_t num_leds_{};
  size_t buffer_size_{};
  uint8_t *buf_{nullptr};   // Raw SPI frame
  uint8_t *base_{nullptr};  // Raw SPI frame with offset to start of color data
  uint8_t address_multiplier_{};
  uint8_t *effect_data_{nullptr};
  float min_mireds_{};
  float max_mireds_{};
};

}  // namespace spi_led_strip
}  // namespace esphome
