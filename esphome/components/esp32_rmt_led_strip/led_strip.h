#pragma once

#ifdef USE_ESP32

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_idf_version.h>
#include <driver/rmt_tx.h>
#include <string>
#include <map>

namespace esphome {
namespace esp32_rmt_led_strip {

struct ChannelMap {
  struct Channel {
    bool exists = false;
    int8_t position = -1;
  };

  std::map<std::string, Channel> channels{
      {"R", Channel()}, {"G", Channel()}, {"B", Channel()}, {"W", Channel()}, {"CW", Channel()}, {"WW", Channel()},
  };

  uint8_t channel_count = -1;

  bool is_rgb() const {
    return this->channels.at("R").exists && this->channels.at("G").exists && this->channels.at("B").exists &&
           !this->channels.at("W").exists && !this->channels.at("CW").exists && !this->channels.at("WW").exists;
  }

  bool is_rgbw() const {
    return this->channels.at("R").exists && this->channels.at("G").exists && this->channels.at("B").exists &&
           this->channels.at("W").exists && !this->channels.at("CW").exists && !this->channels.at("WW").exists;
  }

  bool is_rgbcct() const {
    return this->channels.at("R").exists && this->channels.at("G").exists && this->channels.at("B").exists &&
           !this->channels.at("W").exists && this->channels.at("CW").exists && this->channels.at("WW").exists;
  }
};

struct LedParams {
  rmt_symbol_word_t bit0;
  rmt_symbol_word_t bit1;
  rmt_symbol_word_t reset;
};

enum RGBOrder : uint8_t {  // Deprecated (in favor of channel_map)
  ORDER_NO_SET,
  ORDER_RGB,
  ORDER_RBG,
  ORDER_GRB,
  ORDER_GBR,
  ORDER_BGR,
  ORDER_BRG,
};

class ESP32RMTLEDStripLightOutput : public light::AddressableLight {
 public:
  void setup() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override;

  int32_t size() const override { return this->num_leds_; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    if (this->channel_map_.is_rgbcct()) {
      traits.set_supported_color_modes({light::ColorMode::RGB_COLD_WARM_WHITE});
      // Apply configured mired range (defaults set on construction or via YAML)
      traits.set_min_mireds(this->min_mireds_);
      traits.set_max_mireds(this->max_mireds_);
    } else if (this->channel_map_.is_rgbw()) {
      traits.set_supported_color_modes({light::ColorMode::RGB_WHITE, light::ColorMode::WHITE});
    } else {
      traits.set_supported_color_modes({light::ColorMode::RGB});
    }
    return traits;
  }

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_num_leds(uint16_t num_leds) { this->num_leds_ = num_leds; }
  void set_rgb_order(RGBOrder rgb_order) {
    this->deprecated_settings_.rgb_order_ = rgb_order;
  }  // Deprecated (in favor of channel_map)
  void set_is_rgbw(bool is_rgbw) {
    this->deprecated_settings_.is_rgbw_ = is_rgbw;
  }  // Deprecated (in favor of channel_map)
  void set_is_wrgb(bool is_wrgb) {
    this->deprecated_settings_.is_wrgb_ = is_wrgb;
  }  // Deprecated (in favor of channel_map)
  void set_min_mireds(float min_reds) { this->min_mireds_ = min_reds; }
  void set_max_mireds(float max_mireds) { this->max_mireds_ = max_mireds; }
  void set_use_dma(bool use_dma) { this->use_dma_ = use_dma; }
  void set_use_psram(bool use_psram) { this->use_psram_ = use_psram; }

  /// Set a maximum refresh rate in µs as some lights do not like being updated too often.
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

  void set_led_params(uint32_t bit0_high, uint32_t bit0_low, uint32_t bit1_high, uint32_t bit1_low,
                      uint32_t reset_time_high, uint32_t reset_time_low);

  void set_channel_map(const std::string &map) { this->set_channel_map_(map); }
  void set_rmt_symbols(uint32_t rmt_symbols) { this->rmt_symbols_ = rmt_symbols; }

  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }

  void dump_config() override;

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  size_t get_buffer_size_() const { return this->num_leds_ * this->channel_map_.channel_count; }

  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};
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
  uint16_t num_leds_;

  ChannelMap channel_map_{};
  float min_mireds_{154.0f};
  float max_mireds_{500.0f};

  struct {
    RGBOrder rgb_order_{ORDER_NO_SET};  // Deprecated (in favor of channel_map)
    bool is_rgbw_{false};               // Deprecated (in favor of channel_map)
    bool is_wrgb_{false};               // Deprecated (in favor of channel_map)
  } deprecated_settings_;

  bool use_dma_{false};
  bool use_psram_{false};

  uint32_t last_refresh_{0};
  optional<uint32_t> max_refresh_rate_{};
  bool set_channel_(const std::string &channel_name, int index);
  void set_channel_map_(const std::string &map);
};

}  // namespace esp32_rmt_led_strip
}  // namespace esphome

#endif  // USE_ESP32
