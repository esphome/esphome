#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"


namespace esphome {
namespace vl53l1x {

class VL53L1X;

enum DistanceMode { SHORT, MEDIUM, LONG };

class VL53L1XSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  VL53L1XSensor();

  void setup() override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void update() override;

  void loop() override;

  void set_timing_budget(uint32_t budget) { timing_budget_ = budget; }
  void set_distance_mode(DistanceMode mode) { distance_mode_ = mode; }

  void set_i2c_parent(i2c::I2CComponent* parent);
  void set_i2c_address(uint8_t address);

  void set_retry_budget(uint8_t budget) { retry_budget_ = budget; }

 protected:
  VL53L1X* vl53l1x_{nullptr};
  DistanceMode distance_mode_{DistanceMode::LONG};
  uint32_t timing_budget_{50000};
  uint8_t retry_budget_{5};
  uint8_t retry_count_{0};
};

}  // namespace vl53l1x
}  // namespace esphome
