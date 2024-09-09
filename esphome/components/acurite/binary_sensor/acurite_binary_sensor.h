
#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/acurite/acurite.h"

namespace esphome {
namespace acurite {

class AcuRiteBinarySensor : public Component, public AcuRiteDevice {
 public:
  void update_battery(uint8_t value) override;
  void dump_config() override;

  SUB_BINARY_SENSOR(battery_level)
};

}  // namespace acurite
}  // namespace esphome
