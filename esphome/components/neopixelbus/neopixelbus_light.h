#pragma once

#if defined(USE_ARDUINO) && !defined(CLANG_TIDY)

#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/macros.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/addressable_light.h"

#include "NeoPixelBus.h"

namespace esphome::neopixelbus {

enum class ESPNeoPixelOrder {
  GBWR = 0b11000110,
  GBRW = 0b10000111,
  GBR = 0b10000111,
  GWBR = 0b11001001,
  GRBW = 0b01001011,
  GRB = 0b01001011,
  GWRB = 0b10001101,
  GRWB = 0b01001110,
  BGWR = 0b11010010,
  BGRW = 0b10010011,
  BGR = 0b10010011,
  WGBR = 0b11011000,
  RGBW = 0b00011011,
  RGB = 0b00011011,
  WGRB = 0b10011100,
  RGWB = 0b00011110,
  BWGR = 0b11100001,
  BRGW = 0b01100011,
  BRG = 0b01100011,
  WBGR = 0b11100100,
  RBGW = 0b00100111,
  RBG = 0b00100111,
  WRGB = 0b01101100,
  RWGB = 0b00101101,
  BWRG = 0b10110001,
  BRWG = 0b01110010,
  WBRG = 0b10110100,
  RBWG = 0b00110110,
  WRBG = 0b01111000,
  RWBG = 0b00111001,
};

template<typename T_METHOD, typename T_COLOR_FEATURE, bool has_white>
class NeoPixelBusLightOutputBase : public light::AddressableLight {
 public:
  NeoPixelBus<T_COLOR_FEATURE, T_METHOD> *get_controller() const { return this->controller_; }

  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }

  /// Add some LEDS, can only be called once.
  void add_leds(uint16_t count_pixels, uint8_t pin) {
    this->add_leds(new NeoPixelBus<T_COLOR_FEATURE, T_METHOD>(count_pixels, pin));
  }
  void add_leds(uint16_t count_pixels, uint8_t pin_clock, uint8_t pin_data) {
    this->add_leds(new NeoPixelBus<T_COLOR_FEATURE, T_METHOD>(count_pixels, pin_clock, pin_data));
  }
  void add_leds(uint16_t count_pixels) { this->add_leds(new NeoPixelBus<T_COLOR_FEATURE, T_METHOD>(count_pixels)); }
  void add_leds(NeoPixelBus<T_COLOR_FEATURE, T_METHOD> *controller) {
    this->controller_ = controller;
    // controller gets initialised in setup() - avoid calling twice (crashes with RMT)
    // this->controller_->Begin();
  }

  // ========== INTERNAL METHODS ==========
  void setup() override {
    this->effect_data_ = new uint8_t[this->size()];  // NOLINT
    this->controller_->Begin();
    this->accessor_ = this->internal_accessor_.init(this->controller_->Pixels(), this->effect_data_,
                                                    this->channel_colors_, &this->correction_);
  }

  void write_state(light::LightState *state) override {
    this->mark_shown_();
    this->controller_->Dirty();
    this->controller_->Show();
    this->internal_accessor_.get()->swap_color_data_buffers(this->controller_->Pixels());
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  int32_t size() const override { return this->controller_->PixelCount(); }

  void set_pixel_order(ESPNeoPixelOrder order) {
    uint8_t u_order = static_cast<uint8_t>(order);
    this->channel_colors_.r = (u_order >> 6) & 0b11;
    this->channel_colors_.g = (u_order >> 4) & 0b11;
    this->channel_colors_.b = (u_order >> 2) & 0b11;
    this->channel_colors_.w = has_white ? (u_order >> 0) & 0b11 : light::ChannelColors::NO_WHITE;
  }

 protected:
  Inplace<light::CorrectedBufferColorAccessor> internal_accessor_;
  NeoPixelBus<T_COLOR_FEATURE, T_METHOD> *controller_{nullptr};
  uint8_t *effect_data_{nullptr};
  light::ChannelColors channel_colors_{};
};

template<typename T_METHOD, typename T_COLOR_FEATURE = NeoRgbFeature>
class NeoPixelRGBLightOutput : public NeoPixelBusLightOutputBase<T_METHOD, T_COLOR_FEATURE, false> {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
};

template<typename T_METHOD, typename T_COLOR_FEATURE = NeoRgbwFeature>
class NeoPixelRGBWLightOutput : public NeoPixelBusLightOutputBase<T_METHOD, T_COLOR_FEATURE, true> {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
    return traits;
  }
};

}  // namespace esphome::neopixelbus

#endif  // USE_ARDUINO
