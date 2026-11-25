#pragma once

#include "../systa_bus.h"
#include "esphome/components/sensor/sensor.h"

#include <span>

namespace esphome::systa_bus {

class SystaSolarAquaSensor : public SystaBusListener {
  SUB_SENSOR(temperature_tsa)
  SUB_SENSOR(temperature_tse)
  SUB_SENSOR(temperature_twu)
  SUB_SENSOR(temperature_tw2)
  SUB_SENSOR(pump_speed)

 public:
  void dump_child_config() override;
  void handle_message(std::span<const uint8_t> message) override;
};

}  // namespace esphome::systa_bus
