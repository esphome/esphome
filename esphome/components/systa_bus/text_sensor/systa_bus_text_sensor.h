#pragma once

#include "../systa_bus.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <span>

namespace esphome::systa_bus {

class SystaSolarAquaTextSensor : public SystaBusListener {
  SUB_TEXT_SENSOR(error_code)

 public:
  void dump_config() override;
  void handle_message(std::span<const uint8_t> message) override;
};

}  // namespace esphome::systa_bus
