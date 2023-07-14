#pragma once

#include <driver/spi_master.h>

#include "esphome/components/light/addressable_light.h"

namespace esphome {
namespace esp32_spi_led_strip {

enum RGBOrder : uint8_t {
  ORDER_RGB = 0,
  ORDER_RBG,
  ORDER_GRB,
  ORDER_GBR,
  ORDER_BGR,
  ORDER_BRG,
};

class LedStripSpi : public light::AddressableLight {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void setup() override;
  void write_state(light::LightState *state) override;
  void dump_config() override;
  int32_t size() const override { return this->num_leds_; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({
        light::ColorMode::RGB,
        light::ColorMode::BRIGHTNESS,
    });
    return traits;
  }

  void set_data_pin(uint8_t data_pin) { this->data_pin_ = data_pin; }
  void set_clock_pin(uint8_t clock_pin) { this->clock_pin_ = clock_pin; }
  void set_num_leds(uint16_t num_leds) { this->num_leds_ = num_leds; }
  void set_rgb_order(RGBOrder rgb_order) { this->rgb_order_ = rgb_order; }
  /// Set a maximum refresh rate in µs as some lights do not like being updated too often.
  void set_max_refresh_rate(uint32_t interval_us) { this->max_refresh_rate_ = interval_us; }

  void clear_effect_data() override {
    for (int32_t i = 0; i < this->size(); i++)
      this->effect_data_[i] = 0;
  }

 protected:
  uint8_t data_pin_{};
  uint8_t clock_pin_{};
  uint16_t num_leds_{};
  RGBOrder rgb_order_{RGBOrder::ORDER_RGB};
  uint32_t max_refresh_rate_{};
  uint32_t last_refresh_{};

  uint8_t *effect_data_{};
  uint8_t *buf_{};
  spi_device_handle_t spi_device_{};

  light::ESPColorView get_view_internal(int32_t index) const override;

  bool spi_init_();
  void spi_flush_();
  static constexpr size_t frame_size_ = 4;  // The size of a LED frame
  int buf_size_() const {
    return 0                                        //
           + (this->frame_size_)                    // Start frame
           + (this->frame_size_ * this->num_leds_)  // LED frames
           + (this->frame_size_)                    // Reset frame
           + (this->num_leds_ / 16 + 1)             // Last frame
        ;
  };
};

}  // namespace esp32_spi_led_strip
}  // namespace esphome
