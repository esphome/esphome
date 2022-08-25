#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#define CRC16_ONEWIRE_START 0xFFFF
#define FUNCTION_CODE_READ 0x03
#define SERIAL_NUMBER 0x0000
#define TEMPERATURE_ADDRESS 0x03EA
#define CO2_ADDRESS 0x0424
#define PRESSURE_ADDRESS 0x04B0 

namespace esphome {
namespace ee895 {

/// This class implements support for the ee895 of temperature i2c sensors.
class EE895Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_co2_sensor(sensor::Sensor *co2) { co2_sensor_ = co2; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { pressure_sensor_ = pressure_sensor; }

  float get_setup_priority() const override;
  void setup() override;
  void dump_config() override;
  void update() override;

 protected:
  void write_command(uint16_t addr, uint16_t reg_cnt);
  float read_float();
  uint16_t calc_crc16_(const unsigned char buf[], unsigned char len);
  sensor::Sensor *co2_sensor_;
  sensor::Sensor *temperature_sensor_;
  sensor::Sensor *pressure_sensor_;

  enum ErrorCode { NONE = 0, COMMUNICATION_FAILED, CRC_CHECK_FAILED } error_code_{NONE};
};

}  // namespace ee895
}  // namespace esphome
