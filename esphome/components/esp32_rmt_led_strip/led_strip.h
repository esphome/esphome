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

class ESP32RMTLEDStripLightOutput;

enum RGBOrder : uint8_t {
  ORDER_RGB,
  ORDER_RBG,
  ORDER_GRB,
  ORDER_GBR,
  ORDER_BGR,
  ORDER_BRG,
};

// encoding types sourced from Vishay reference
// https://www.vishay.com/docs/80071/dataform.pdf
enum Encoding : uint8_t {
  ENCODING_PULSE_LENGTH,
  ENCODING_PULSE_DISTANCE,
  ENCODING_BI_PHASE,
};

using Generator = void(const ESP32RMTLEDStripLightOutput &light, int led_num, const uint8_t *src_buf,
                       rmt_item32_t *dest_buf, light::LightState *state);

class ESP32RMTLEDStripLightOutput : public light::AddressableLight {
 public:
  void setup() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override;

  int32_t size() const override { return this->num_leds_; }
  light::LightTraits get_traits() override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_num_leds(uint16_t num_leds) { this->num_leds_ = num_leds; }
  bool get_is_rgbw() const { return this->is_rgbw_; }
  void set_is_rgbw(bool is_rgbw) {
    if (is_rgbw) {
      this->set_supported_color_modes({light::ColorMode::RGB_WHITE, light::ColorMode::WHITE});
    } else {
      this->set_supported_color_modes({light::ColorMode::RGB});
    }
  }
  // used to set rgbw backing buffer without exposing the "RGB_WHITE" mode trait to HA
  void set_internal_is_rgbw(bool is_rgbw) { this->is_rgbw_ = is_rgbw; }
  void set_supported_color_modes(const std::set<esphome::light::ColorMode> &color_modes);
  void set_is_inverted(bool is_inverted) { this->is_inverted_ = is_inverted; }
  Encoding get_encoding() const { return this->encoding_; }
  void set_encoding(Encoding encoding) { this->encoding_ = encoding; }
  void set_allow_partial_updates(bool allow) { this->allow_partial_updates_ = allow; }

  /// Set a maximum refresh rate in µs as some lights do not like being updated too often.
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

  const rmt_item32_t &get_bit0() const { return this->bit0_; }
  const rmt_item32_t &get_bit1() const { return this->bit1_; }
  void set_led_params(uint32_t bit0_high, uint32_t bit0_low, uint32_t bit1_high, uint32_t bit1_low);
  uint32_t get_sync_start() const { return this->sync_start_; }
  void set_sync_start(uint32_t sync_start);
  void set_intermission(uint32_t intermission) { this->intermission_ = intermission; }

  RGBOrder get_rgb_order() const { return this->rgb_order_; }
  void set_rgb_order(RGBOrder rgb_order) { this->rgb_order_ = rgb_order; }
  void set_rmt_channel(rmt_channel_t channel) { this->channel_ = channel; }
  void set_rmt_generator(std::function<Generator> gen) { this->rmt_generator_ = std::move(gen); }

  void set_bits_per_command(uint32_t bits) { this->bits_per_command_ = bits; }

  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }

  void dump_config() override;

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  size_t get_bytes_per_led_() const { return 3 + this->is_rgbw_; }
  size_t get_bits_per_command_() const;
  size_t get_items_per_command_() const;
  size_t get_buffer_size_() const { return this->num_leds_ * this->get_bytes_per_led_(); }
  size_t get_rmt_buffer_size_() const;

  static uint32_t get_cycle_count_for_micros_delay(uint32_t delay);
  static uint32_t get_rmt_item_count_for_micros_delay(uint32_t delay);
  static void generate_rmt_items_for_micros_delay(rmt_item32_t *dest, uint32_t delay);

  uint8_t *get_current_buf_() const { return buf_[this->current_buf_]; }
  uint8_t *get_old_buf_() const { return buf_[!this->current_buf_]; }
  void swap_buf_() {
    if (this->allow_partial_updates_)
      this->current_buf_ = !this->current_buf_;
  }
  bool bufs_same_at_index_(int i) const;
  bool rmt_write_items_(size_t len, bool wait_tx_done = false);

  static Generator default_rmt_generate;

  uint8_t *buf_[2];
  int current_buf_ = 0;
  uint8_t *effect_data_{nullptr};
  rmt_item32_t *rmt_buf_{nullptr};

  uint8_t pin_;
  uint16_t num_leds_;
  bool is_rgbw_;
  std::set<light::ColorMode> supported_color_modes_;
  bool is_inverted_;
  bool allow_partial_updates_;

  uint32_t sync_start_ = 0;
  uint32_t intermission_ = 0;
  rmt_item32_t bit0_, bit1_;
  RGBOrder rgb_order_;
  Encoding encoding_ = ENCODING_PULSE_LENGTH;
  std::function<Generator> rmt_generator_ = default_rmt_generate;
  rmt_channel_t channel_;

  uint32_t bits_per_command_ = 0;

  uint32_t last_refresh_{0};
  optional<uint32_t> max_refresh_rate_{};
};

}  // namespace esp32_rmt_led_strip
}  // namespace esphome

#endif  // USE_ESP32
