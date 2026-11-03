#pragma once

#include "esphome/components/max170xx/max170xx.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::max17043 {

class MAX17043Component final : public max170xx::MAX170xxComponent<max170xx::Model::MODEL_MAX17043> {
  SUB_SENSOR(voltage)
  SUB_SENSOR(battery_remaining)

 public:
  void dump_config() override;
  void update() override;
};

}  // namespace esphome::max17043
