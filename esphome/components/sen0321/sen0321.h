#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

// ref:
// https://github.com/DFRobot/DFRobot_OzoneSensor

namespace esphome {
namespace sen0321_sensor {
// Sensor Mode
// While passive is supposedly supported, it does not appear to work reliably.
static const uint8_t SENSOR_MODE_REGISTER = 0x03;
static const uint8_t SENSOR_MODE_AUTO = 0x00;
static const uint8_t SENSOR_MODE_PASSIVE = 0x01;
static const uint8_t SET_REGISTER = 0x04;

// Each register is 2 wide, so 0x07-0x08 for passive, or 0x09-0x0A for auto
// First register is high bits, next low.
static const uint8_t SENSOR_PASS_READ_REG = 0x07;
static const uint8_t SENSOR_AUTO_READ_REG = 0x09;

class Sen0321Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void update() override;
  void dump_config() override;
  void setup() override;

 protected:
  void read_data_();
};

}  // namespace sen0321_sensor
}  // namespace esphome
