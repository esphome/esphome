#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

class VL53L1X;

namespace esphome {
namespace vl53l1x {

enum DistanceMode { SHORT, MEDIUM, LONG };

class VL53L1XSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  VL53L1XSensor();

  void setup() override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void update() override;

  void loop() override;

  void set_timing_budget(uint32_t budget) { timingBudget_ = budget; }
  void set_distance_mode(DistanceMode mode) { distanceMode_ = mode; }

  void set_i2c_parent(i2c::I2CComponent* parent);
  void set_i2c_address(uint8_t address);

 protected:
  VL53L1X* vl53l1x_{nullptr};
  DistanceMode distanceMode_{DistanceMode::LONG};
  uint32_t timingBudget_{50000};
};

}  // namespace vl53l1x
}  // namespace esphome
