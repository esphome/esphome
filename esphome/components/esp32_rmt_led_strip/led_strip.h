#pragma once

#ifdef USE_ESP32

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/channel_colors.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_idf_version.h>
#include <driver/rmt_tx.h>

namespace esphome::esp32_rmt_led_strip {

struct LedParams {
  rmt_symbol_word_t bit0;
  rmt_symbol_word_t bit1;
  rmt_symbol_word_t reset;
};

class ESP32RMTLEDStripLightOutput final : public light::AddressableLight {
 public:
  ESP32RMTLEDStripLightOutput(size_t num_leds, light::ChannelColors channel_colors)
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
  void set_inverted(bool inverted) { this->invert_out_ = inverted; }
  void set_use_dma(bool use_dma) { this->use_dma_ = use_dma; }
  void set_use_psram(bool use_psram) { this->use_psram_ = use_psram; }

  /// Set a maximum refresh rate in µs as some lights do not like being updated too often.
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

  void set_led_params(uint32_t bit0_high, uint32_t bit0_low, uint32_t bit1_high, uint32_t bit1_low,
                      uint32_t reset_time_high, uint32_t reset_time_low);

  void set_rmt_symbols(uint32_t rmt_symbols) { this->rmt_symbols_ = rmt_symbols; }

  void dump_config() override;

 protected:
  light::PackedColorBuffer buffer_;

  LedParams params_;
  rmt_channel_handle_t channel_{nullptr};
  rmt_encoder_handle_t encoder_{nullptr};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
  uint8_t *rmt_buf_{nullptr};
#else
  rmt_symbol_word_t *rmt_buf_{nullptr};
#endif
  uint32_t rmt_symbols_{48};
  uint8_t pin_;
  bool use_dma_{false};
  bool use_psram_{false};
  bool invert_out_{false};

  uint32_t last_refresh_{0};
  optional<uint32_t> max_refresh_rate_{};
};

}  // namespace esphome::esp32_rmt_led_strip

#endif  // USE_ESP32
