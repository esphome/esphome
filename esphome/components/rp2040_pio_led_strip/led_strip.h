#pragma once

#ifdef USE_RP2040

#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_output.h"

#include <hardware/dma.h>
#include <hardware/pio.h>
#include <hardware/structs/pio.h>
#include <pico/stdio.h>
#include <map>

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
  CHIPSET_WS2812,
  CHIPSET_WS2812B,
  CHIPSET_SK6812,
  CHIPSET_SM16703,
  CHIPSET_APA102,
  CHIPSET_CUSTOM = 0xFF,
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

using init_fn = void (*)(PIO pio, uint sm, uint offset, uint pin, float freq);

class RP2040PIOLEDStripLightOutput : public light::AddressableLight {
 public:
  void setup() override;
  void write_state(light::LightState *state) override;
  float get_setup_priority() const override;

  int32_t size() const override { return this->num_leds_; }
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    this->is_rgbw_ ? traits.set_supported_color_modes({light::ColorMode::RGB_WHITE, light::ColorMode::WHITE})
                   : traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_num_leds(uint32_t num_leds) { this->num_leds_ = num_leds; }
  void set_is_rgbw(bool is_rgbw) { this->is_rgbw_ = is_rgbw; }

  void set_max_refresh_rate(float interval_us) { this->max_refresh_rate_ = interval_us; }

  void set_pio(int pio_num) { pio_num ? this->pio_ = pio1 : this->pio_ = pio0; }
  void set_program(const pio_program_t *program) { this->program_ = program; }
  void set_init_function(init_fn init) { this->init_ = init; }

  void set_chipset(Chipset chipset) { this->chipset_ = chipset; };
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
  uint8_t *effect_data_{nullptr};

  uint8_t pin_;
  uint32_t num_leds_;
  bool is_rgbw_;

  pio_hw_t *pio_;
  uint sm_;
  uint dma_chan_;
  dma_channel_config dma_config_;

  RGBOrder rgb_order_{ORDER_RGB};
  Chipset chipset_{CHIPSET_CUSTOM};

  uint32_t last_refresh_{0};
  float max_refresh_rate_;

  const pio_program_t *program_;
  init_fn init_;

 private:
  inline static int num_instance_[2];
  inline static std::map<Chipset, bool> conf_count_;
  inline static std::map<Chipset, int> chipset_offsets_;
};

}  // namespace rp2040_pio_led_strip
}  // namespace esphome

#endif  // USE_RP2040
