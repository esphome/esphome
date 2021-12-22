#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light_effect.h"
#include "esphome/components/uart/uart.h"

#include <vector>

namespace esphome {
namespace adalight {

class AdalightLightEffect : public light::AddressableLightEffect, public uart::UARTDevice {
 public:
  AdalightLightEffect(const std::string &name);

 public:
  void start() override;
  void stop() override;
  void apply(light::AddressableLight &it, const Color &current_color) override;

 protected:
  enum Frame {
    INVALID,
    PARTIAL,
    CONSUMED,
  };

  unsigned int get_frame_size_(int led_count) const;
  void reset_frame_(light::AddressableLight &it);
  void blank_all_leds_(light::AddressableLight &it);
  Frame parse_frame_(light::AddressableLight &it);

 protected:
  uint32_t last_ack_{0};
  uint32_t last_byte_{0};
  uint32_t last_reset_{0};
  std::vector<uint8_t> frame_;
};

}  // namespace adalight
}  // namespace esphome
