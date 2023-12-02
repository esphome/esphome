#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/macros.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/color.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/addressable_light.h"

#include "NeoPixelBus.h"

namespace esphome {
namespace neopixelbus {

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

template<typename T_METHOD, typename T_COLOR_FEATURE>
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
    for (int i = 0; i < this->size(); i++) {
      (*this)[i] = Color(0, 0, 0, 0);
    }

    this->effect_data_ = new uint8_t[this->size()];  // NOLINT
    this->controller_->Begin();
  }

  void write_state(light::LightState *state) override {
    this->mark_shown_();
    this->controller_->Dirty();

    this->controller_->Show();
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  int32_t size() const override { return this->controller_->PixelCount(); }

  void set_pixel_order(ESPNeoPixelOrder order) {
    uint8_t u_order = static_cast<uint8_t>(order);
    this->rgb_offsets_[0] = (u_order >> 6) & 0b11;
    this->rgb_offsets_[1] = (u_order >> 4) & 0b11;
    this->rgb_offsets_[2] = (u_order >> 2) & 0b11;
    this->rgb_offsets_[3] = (u_order >> 0) & 0b11;
  }

 protected:
  NeoPixelBus<T_COLOR_FEATURE, T_METHOD> *controller_{nullptr};
  uint8_t *effect_data_{nullptr};
  uint8_t rgb_offsets_[4]{0, 1, 2, 3};
};

template<typename T_METHOD, typename T_COLOR_FEATURE = NeoRgbFeature>
class NeoPixelRGBLightOutput : public NeoPixelBusLightOutputBase<T_METHOD, T_COLOR_FEATURE> {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {  // NOLINT
    uint8_t *base = this->controller_->Pixels() + 3ULL * index;
    return light::ESPColorView(base + this->rgb_offsets_[0], base + this->rgb_offsets_[1], base + this->rgb_offsets_[2],
                               nullptr, this->effect_data_ + index, &this->correction_);
  }
};

template<typename T_METHOD, typename T_COLOR_FEATURE = NeoRgbwFeature>
class NeoPixelRGBWLightOutput : public NeoPixelBusLightOutputBase<T_METHOD, T_COLOR_FEATURE> {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
    return traits;
  }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {  // NOLINT
    uint8_t *base = this->controller_->Pixels() + 4ULL * index;
    return light::ESPColorView(base + this->rgb_offsets_[0], base + this->rgb_offsets_[1], base + this->rgb_offsets_[2],
                               base + this->rgb_offsets_[3], this->effect_data_ + index, &this->correction_);
  }
};

}  // namespace neopixelbus
}  // namespace esphome

#endif  // USE_ARDUINO
