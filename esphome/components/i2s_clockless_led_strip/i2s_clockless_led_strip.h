#pragma once

#ifdef USE_ESP32
#include <soc/soc_caps.h>
#endif

#if defined(USE_ESP32) && SOC_I2S_SUPPORTS_TDM

#include <atomic>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/channel_colors.h"

#include <driver/i2s_types.h>

namespace esphome::i2s_clockless_led_strip {

class I2SClocklessLedStrip final : public light::AddressableLight {
 public:
  I2SClocklessLedStrip(uint8_t pin, uint16_t num_leds, light::ChannelColors channel_colors);

  void dump_config() override;

  float get_setup_priority() const override;
  void setup() override;

  light::LightTraits get_traits() override;

  int32_t size() const override { return this->num_leds_; }
  void clear_effect_data() override;
  void write_state(light::LightState *state) override;

 protected:
  static bool i2s_on_sent_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);

  bool allocate_buffers_();
  light::ESPColorView get_view_internal(int32_t index) const override;

  const uint8_t pin_;
  const uint16_t num_leds_;
  const light::ChannelColors channel_colors_;
  const size_t color_data_bytes_;

  uint8_t *effect_data_{nullptr};
  uint8_t *color_data_{nullptr};
  uint8_t *i2s_data_{nullptr};
  std::atomic<bool> i2s_data_ready_{false};
  size_t i2s_data_sent_{0};

  i2s_chan_handle_t tx_handle_{};
};

}  // namespace esphome::i2s_clockless_led_strip

#endif
