#pragma once

#ifdef USE_ESP32
#include <soc/soc_caps.h>
#endif

#if defined(USE_ESP32) && SOC_I2S_SUPPORTS_TDM

#include <atomic>
#include <cstdint>
#include <cstring>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/channel_colors.h"

#include <driver/i2s_types.h>

namespace esphome::i2s_clockless_led_strip {

// Each byte of LED strip data becomes one 24-bit I2S sample
static constexpr size_t I2S_BYTES_PER_SAMPLE = 3;
/// The color and effect data rounded up to a word, so the I2S data after it that the ISR copies is word aligned
static constexpr size_t i2s_data_offset(uint16_t num_leds, uint8_t bytes_per_led) {
  return (light::BufferColorAccessor::buffer_bytes(num_leds, bytes_per_led) + 3) / 4 * 4;
}
/// The whole block for the constructor, the I2S data holding I2S_BYTES_PER_SAMPLE bytes per color byte
static constexpr size_t buffer_bytes(uint16_t num_leds, uint8_t bytes_per_led) {
  return i2s_data_offset(num_leds, bytes_per_led) + num_leds * bytes_per_led * I2S_BYTES_PER_SAMPLE;
}

class I2SClocklessLedStrip final : public light::AddressableLight {
 public:
  /// `buffer` is a word aligned block of buffer_bytes().
  I2SClocklessLedStrip(uint8_t *buffer, uint8_t pin, uint16_t num_leds, light::ChannelColors channel_colors);

  void dump_config() override;

  float get_setup_priority() const override;
  void setup() override;

  light::LightTraits get_traits() override { return this->internal_accessor_.channel_colors.traits(); }

  int32_t size() const override { return this->num_leds_; }
  void clear_effect_data() override { this->internal_accessor_.clear_effect_data(this->num_leds_); }
  void write_state(light::LightState *state) override;

 protected:
  static bool i2s_on_sent_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);

  light::CorrectedBufferColorAccessor internal_accessor_;
  const size_t color_data_bytes_;
  uint8_t *i2s_data_{nullptr};
  size_t i2s_data_sent_{0};
  i2s_chan_handle_t tx_handle_{};
  const uint16_t num_leds_;
  const uint8_t pin_;
  std::atomic<bool> i2s_data_ready_{false};
};

}  // namespace esphome::i2s_clockless_led_strip

#endif
