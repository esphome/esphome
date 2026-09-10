#pragma once

#if defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_LIBRETINY)

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/light/addressable_light.h"

#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP32_RAW_PIN_ORDER
#define FASTLED_RMT_BUILTIN_DRIVER true

// Avoid annoying compiler messages
#define FASTLED_INTERNAL

#include "FastLED.h"

namespace esphome::fastled_base {

class FastLEDLightOutput final : public light::AddressableLight, protected light::ESPColorBuffer {
 public:
  FastLEDLightOutput(size_t num_leds, CLEDController *controller)
      : ESPColorBuffer(num_leds),
        controller_(controller),
        led_data_(new CRGB[num_leds]{CRGB::Black}),
        effect_data_(new uint8_t[num_leds]{0}) {}

  light::ESPColorBuffer &buffer() override { return *this; }

  /// Only for custom effects: Get the internal controller.
  CLEDController *get_controller() const { return this->controller_; }

  /// Set a maximum refresh rate in µs as some lights do not like being updated too often.
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

  template<ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER, uint32_t SPI_DATA_RATE>
  static CLEDController *make_controller() {
    switch (CHIPSET) {
      case LPD8806: {
        static LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> controller;
        return &controller;
      }
      case WS2801: {
        static WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> controller;
        return &controller;
      }
      case WS2803: {
        static WS2803Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> controller;
        return &controller;
      }
      case SM16716: {
        static SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> controller;
        return &controller;
      }
      case P9813: {
        static P9813Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> controller;
        return &controller;
      }
      case DOTSTAR:
      case APA102: {
        static APA102Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> controller;
        return &controller;
      }
      case SK9822: {
        static SK9822Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> controller;
        return &controller;
      }
    }
  }

  template<ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN> static CLEDController *make_controller() {
    switch (CHIPSET) {
      case LPD8806: {
        static LPD8806Controller<DATA_PIN, CLOCK_PIN> controller;
        return &controller;
      }
      case WS2801: {
        static WS2801Controller<DATA_PIN, CLOCK_PIN> controller;
        return &controller;
      }
      case WS2803: {
        static WS2803Controller<DATA_PIN, CLOCK_PIN> controller;
        return &controller;
      }
      case SM16716: {
        static SM16716Controller<DATA_PIN, CLOCK_PIN> controller;
        return &controller;
      }
      case P9813: {
        static P9813Controller<DATA_PIN, CLOCK_PIN> controller;
        return &controller;
      }
      case DOTSTAR:
      case APA102: {
        static APA102Controller<DATA_PIN, CLOCK_PIN> controller;
        return &controller;
      }
      case SK9822: {
        static SK9822Controller<DATA_PIN, CLOCK_PIN> controller;
        return &controller;
      }
    }
  }

  template<ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER>
  static CLEDController *make_controller() {
    switch (CHIPSET) {
      case LPD8806: {
        static LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> controller;
        return &controller;
      }
      case WS2801: {
        static WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> controller;
        return &controller;
      }
      case WS2803: {
        static WS2803Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> controller;
        return &controller;
      }
      case SM16716: {
        static SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> controller;
        return &controller;
      }
      case P9813: {
        static P9813Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> controller;
        return &controller;
      }
      case DOTSTAR:
      case APA102: {
        static APA102Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> controller;
        return &controller;
      }
      case SK9822: {
        static SK9822Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> controller;
        return &controller;
      }
    }
  }

  template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
  static CLEDController *make_controller() {
    static CHIPSET<DATA_PIN, RGB_ORDER> controller;
    return &controller;
  }

  template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN>
  static CLEDController *make_controller() {
    static CHIPSET<DATA_PIN, RGB> controller;
    return &controller;
  }

  template<template<uint8_t DATA_PIN> class CHIPSET, uint8_t DATA_PIN> static CLEDController *make_controller() {
    static CHIPSET<DATA_PIN> controller;
    return &controller;
  }

  template<template<EOrder RGB_ORDER> class CHIPSET, EOrder RGB_ORDER> static CLEDController *make_controller() {
    static CHIPSET<RGB_ORDER> controller;
    return &controller;
  }

  template<template<EOrder RGB_ORDER> class CHIPSET> static CLEDController *make_controller() {
    static CHIPSET<RGB> controller;
    return &controller;
  }

#ifdef FASTLED_HAS_BLOCKLESS
  template<EBlockChipsets CHIPSET, int NUM_LANES, EOrder RGB_ORDER> static CLEDController *make_controller() {
    switch (CHIPSET) {
#ifdef PORTA_FIRST_PIN
      case WS2811_PORTA:
        return new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER>();
      case WS2811_400_PORTA:
        return new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(800), NS(800), NS(900), RGB_ORDER>();
      case WS2813_PORTA:
        return new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER, 0,
                                                  false, 300>();
      case TM1803_PORTA:
        return new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(700), NS(1100), NS(700), RGB_ORDER>();
      case UCS1903_PORTA:
        return new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(500), NS(1500), NS(500), RGB_ORDER>();
#endif
    }
  }

  template<EBlockChipsets CHIPSET, int NUM_LANES> static CLEDController *make_controller() {
    return make_controller<CHIPSET, NUM_LANES, GRB>();
  }
#endif

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void setup() override;
  void dump_config() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  bool is_all_black() const override {
    for (size_t i = 0; i < this->num_leds_; i++) {
      if (this->led_data_[i].r != 0 || this->led_data_[i].g != 0 || this->led_data_[i].b != 0) {
        return false;
      }
    }
    return true;
  }

  void clear_effect_data() override { clear_effect_data_internal_(this->effect_data_, this->num_leds_); }

  light::ESPColorView get_color_view_(size_t index) override {
    return light::ESPColorView{&this->led_data_[index].r,  &this->led_data_[index].g,
                               &this->led_data_[index].b,  nullptr,
                               &this->effect_data_[index], &this->correction_};
  }

  CLEDController *const controller_;
  CRGB *const led_data_;
  uint8_t *const effect_data_;

  uint32_t last_refresh_{0};
  optional<uint32_t> max_refresh_rate_{};
};

}  // namespace esphome::fastled_base

#endif  // USE_ARDUINO
