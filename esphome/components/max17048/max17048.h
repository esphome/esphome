#pragma once

#include "esphome/components/max170xx/max170xx.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::max17048 {

class MAX17048Component final : public max170xx::MAX170xxComponent<max170xx::Model::MODEL_MAX17048> {
  SUB_SENSOR(battery_voltage)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(rate)

 public:
  void dump_config() override;
  void update() override;
};

}  // namespace esphome::max17048
