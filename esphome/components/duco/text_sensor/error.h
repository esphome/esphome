#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/components/text_sensor/text_sensor.h"

#include "../duco.h"

namespace esphome::duco {

class DucoError : public DucoDevice, public text_sensor::TextSensor {
 public:
  explicit DucoError(uint8_t address) : address_(address) {}

  void update() override;
  void receive_response(const DucoMessage &message) override;

 protected:
  static constexpr size_t MAX_ERROR_TEXT_LENGTH = 255;

  uint8_t address_;
};

}  // namespace esphome::duco
