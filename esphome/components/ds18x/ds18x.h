#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ds248x/ds248x.h"

using namespace esphome::ds248x;

namespace esphome {
namespace ds18x {

class DS18xTemperatureSensor : public DS248xSensor {
 public:
  /// Get the set resolution for this sensor.
  uint8_t get_resolution() const;
  /// Set the resolution for this sensor.
  void set_resolution(uint8_t resolution);
  
  float get_temp_c();
  float get_value();

  bool setup_sensor();

  bool update();
  void add_conversion_commands(std::set<uint8_t> &commands);

 protected:
   uint8_t resolution_;
};

}
}