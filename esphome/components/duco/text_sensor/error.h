#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/text_sensor/text_sensor.h"

#include "../duco.h"

namespace esphome::duco {

class DucoError : public DucoDevice, public text_sensor::TextSensor {
 public:
  void set_address(uint8_t address) { this->address_ = address; }
  void setup() override;
  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  static constexpr size_t MAX_ERROR_TEXT_LENGTH = 255;

  uint8_t address_{1};
  std::array<char, MAX_ERROR_TEXT_LENGTH + 1> error_text_{};
};

}  // namespace esphome::duco
