#pragma once

#ifdef USE_ESP32

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <driver/gpio.h>
#include <driver/rmt.h>
#include <esp_err.h>

namespace esphome {
namespace esp32_rmt_led_strip {

enum RGBOrder : uint8_t {
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
    if (this->is_rgbw_ || this->is_wrgb_) {
      traits.set_supported_color_modes({light::ColorMode::RGB_WHITE, light::ColorMode::WHITE});
    } else {
      traits.set_supported_color_modes({light::ColorMode::RGB});
    }
    return traits;
  }

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_num_leds(uint16_t num_leds) { this->num_leds_ = num_leds; }
  void set_is_rgbw(bool is_rgbw) { this->is_rgbw_ = is_rgbw; }
  void set_is_wrgb(bool is_wrgb) { this->is_wrgb_ = is_wrgb; }

  /// Set a maximum refresh rate in µs as some lights do not like being updated too often.
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

  void set_led_params(uint32_t bit0_high, uint32_t bit0_low, uint32_t bit1_high, uint32_t bit1_low);

  void set_rgb_order(RGBOrder rgb_order) { this->rgb_order_ = rgb_order; }
  void set_rmt_channel(rmt_channel_t channel) { this->channel_ = channel; }

  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }

  void dump_config() override;

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  size_t get_buffer_size_() const { return this->num_leds_ * (this->is_rgbw_ || this->is_wrgb_ ? 4 : 3); }

  uint8_t *buf_{nullptr};
  uint8_t *effect_data_{nullptr};
  rmt_item32_t *rmt_buf_{nullptr};

  uint8_t pin_;
  uint16_t num_leds_;
  bool is_rgbw_;
  bool is_wrgb_;

  rmt_item32_t bit0_, bit1_;
  RGBOrder rgb_order_;
  rmt_channel_t channel_;

  uint32_t last_refresh_{0};
  optional<uint32_t> max_refresh_rate_{};
};

}  // namespace esp32_rmt_led_strip
}  // namespace esphome

#endif  // USE_ESP32
