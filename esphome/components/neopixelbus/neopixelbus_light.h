#pragma once

#if defined(USE_ARDUINO) && !defined(CLANG_TIDY)

#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/macros.h"
#include "esphome/components/light/channel_colors.h"
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

constexpr light::ChannelColors to_channel_colors(ESPNeoPixelOrder order, bool has_white) {
  uint8_t u_order = static_cast<uint8_t>(order);
  return {
      .r = (u_order >> 6) & 0b11,
      .g = (u_order >> 4) & 0b11,
      .b = (u_order >> 2) & 0b11,
      .w = has_white ? (u_order >> 0) & 0b11 : light::ChannelColors::NO_WHITE,
  };
}

template<typename T_METHOD, typename T_COLOR_FEATURE, bool HAS_WHITE>
class NeoPixelBusLightOutputBase : public light::AddressableLight, protected light::ESPColorBuffer {
 public:
  NeoPixelBusLightOutputBase(size_t num_leds, ESPNeoPixelOrder order, uint8_t pin)
      : NeoPixelBusLightOutputBase(new NeoPixelBus<T_COLOR_FEATURE, T_METHOD>(num_leds, pin), order) {}

  NeoPixelBusLightOutputBase(size_t num_leds, ESPNeoPixelOrder order, uint8_t pin_clock, uint8_t pin_data)
      : NeoPixelBusLightOutputBase(new NeoPixelBus<T_COLOR_FEATURE, T_METHOD>(num_leds, pin_clock, pin_data), order) {}

  light::ESPColorBuffer &buffer() override { return *this; }

  NeoPixelBus<T_COLOR_FEATURE, T_METHOD> *get_controller() const { return this->controller_; }

  // ========== INTERNAL METHODS ==========
  void setup() override { this->controller_->Begin(); }

  void write_state(light::LightState *state) override {
    this->mark_shown_();
    this->controller_->Dirty();

    this->controller_->Show();
  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  using Controller = NeoPixelBus<T_COLOR_FEATURE, T_METHOD>;

  NeoPixelBusLightOutputBase(Controller *controller, ESPNeoPixelOrder order)
      : ESPColorBuffer(controller->PixelCount()),
        controller_(controller),
        effect_data_(new uint8_t[controller->PixelCount()]{0}),
        channel_colors_(to_channel_colors(order)) {}

  bool is_all_black() const override {
    return is_all_black_internal_(this->controller_->Pixels(), this->num_leds, this->channel_colors_,
                                  HAS_WHITE ? 4 : 3);
  }

  void clear_effect_data() override { clear_effect_data_internal_(this->effect_data_, this->num_leds_); }

  light::ESPColorView get_color_view_(size_t index) override {
    return get_color_view_internal_(index, this->controller_->Pixels(), this->effect_data_, this->channel_colors,
                                    HAS_WHITE ? 4 : 3, this->correction_);
  }

  Controller *const controller_;
  uint8_t *const effect_data_;
  ChannelColors const channel_colors_;
};

template<typename T_METHOD, typename T_COLOR_FEATURE = NeoRgbFeature>
class NeoPixelRGBLightOutput : public NeoPixelBusLightOutputBase<T_METHOD, T_COLOR_FEATURE> {
 public:
  NeoPixelRGBLightOutput(size_t num_leds, ESPNeoPixelOrder order, uint8_t pin)
      : NeoPixelBusLightOutputBase(num_leds, order, pin) {}

  NeoPixelRGBLightOutput(size_t num_leds, ESPNeoPixelOrder order, uint8_t pin_clock, uint8_t pin_data)
      : NeoPixelBusLightOutputBase(num_leds, order, pin_clock, pin_data) {}

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
};

template<typename T_METHOD, typename T_COLOR_FEATURE = NeoRgbwFeature>
class NeoPixelRGBWLightOutput : public NeoPixelBusLightOutputBase<T_METHOD, T_COLOR_FEATURE> {
 public:
  NeoPixelRGBWLightOutput(size_t num_leds, ESPNeoPixelOrder order, uint8_t pin)
      : NeoPixelBusLightOutputBase(num_leds, order, pin) {}

  NeoPixelRGBWLightOutput(size_t num_leds, ESPNeoPixelOrder order, uint8_t pin_clock, uint8_t pin_data)
      : NeoPixelBusLightOutputBase(num_leds, order, pin_clock, pin_data) {}

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
    return traits;
  }
};

}  // namespace esphome::neopixelbus

#endif  // USE_ARDUINO
