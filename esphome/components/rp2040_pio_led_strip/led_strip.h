#pragma once

#ifdef USE_RP2040

#include "esphome/core/component.h"
#include "esphome/core/color.h"
#include "esphome/core/helpers.h"

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"

#include "pico/stdio.h"
#include <hardware/structs/pio.h>
#include <hardware/pio.h>

namespace esphome {
namespace rp2040_pio_led_strip {

enum RGBOrder : uint8_t {
  ORDER_RGB,
  ORDER_RBG,
  ORDER_GRB,
  ORDER_GBR,
  ORDER_BGR,
  ORDER_BRG,
};

enum Chipset : uint8_t {
  WS2812,
  WS2812B,
  SK6812,
  SM16703,
};

inline const char *rgb_order_to_string(RGBOrder order) {
  switch (order) {
    case ORDER_RGB:
      return "RGB";
    case ORDER_RBG:
      return "RBG";
    case ORDER_GRB:
      return "GRB";
    case ORDER_GBR:
      return "GBR";
    case ORDER_BGR:
      return "BGR";
    case ORDER_BRG:
      return "BRG";
    default:
      return "UNKNOWN";
  }
}

class RP2040PIOLEDStripLightOutput : public light::AddressableLight {
 public:
  void setup() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override;

  int32_t size() const override { return this->num_leds_; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    this->is_rgbw_ ? traits.set_supported_color_modes({light::ColorMode::RGB, light::ColorMode::RGB_WHITE})
                   : traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_num_leds(uint32_t num_leds) { this->num_leds_ = num_leds; }
  void set_is_rgbw(bool is_rgbw) { this->is_rgbw_ = is_rgbw; }

  void set_max_refresh_rate(float interval_us) { this->max_refresh_rate_ = interval_us; }

  void set_pio(pio_hw_t *pio) { this->pio_ = pio; }

  void set_chipset(Chipset chipset) { this->chipset_ = chipset; }

  void set_rgb_order(RGBOrder rgb_order) { this->rgb_order_ = rgb_order; }
  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++) {
      this->effect_data_[i] = 0;
    }
  }

  void dump_config() override;

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override;

  size_t get_buffer_size_() const { return this->num_leds_ * (3 + this->is_rgbw_); }

  uint8_t *buf_{nullptr};
  uint8_t *write_buf_{nullptr};
  uint8_t *effect_data_{nullptr};

  Chipset chipset_{WS2812};
  const pio_program *pio_program_{nullptr};

  uint8_t pin_;
  uint32_t num_leds_;
  bool is_rgbw_;

  pio_hw_t *pio_;
  uint sm_;

  RGBOrder rgb_order_{ORDER_RGB};

  uint32_t last_refresh_{0};
  float max_refresh_rate_;
};

}  // namespace rp2040_pio_led_strip
}  // namespace esphome

#endif  // USE_RP2040
