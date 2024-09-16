// for Honeywell ABP sensor
// adapting code from https://github.com/vwls/Honeywell_pressure_sensors
#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"
#include "esphome/core/component.h"

namespace esphome {
namespace honeywellabp2_i2c {

enum ABP2TRANFERFUNCTION { ABP2_TRANS_FUNC_A = 0, ABP2_TRANS_FUNC_B = 1 };

class HONEYWELLABP2Sensor : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { this->pressure_sensor_ = pressure_sensor; };
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; };
  void loop() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; };
  void dump_config() override;

  void read_sensor_data();
  void start_measurement();
  bool is_measurement_ready();
  void measurement_timeout();

  float get_pressure();
  float get_temperature();

  void set_min_pressure(float min_pressure) { this->min_pressure_ = min_pressure; };
  void set_max_pressure(float max_pressure) { this->max_pressure_ = max_pressure; };
  void set_transfer_function(ABP2TRANFERFUNCTION transfer_function);

 protected:
  float min_pressure_ = 0.0;
  float max_pressure_ = 0.0;
  ABP2TRANFERFUNCTION transfer_function_ = ABP2_TRANS_FUNC_A;

  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};

  const float max_count_a_ = 15099494.4;  // (90% of 2^24 counts or 0xE66666)
  const float min_count_a_ = 1677721.6;   // (10% of 2^24 counts or 0x19999A)
  const float max_count_b_ = 11744051.2;  // (70% of 2^24 counts or 0xB33333)
  const float min_count_b_ = 5033164.8;   // (30% of 2^24 counts or 0x4CCCCC)

  float max_count_;
  float min_count_;
  bool measurement_running_ = false;

  uint8_t raw_data_[7];                      // holds output data
  uint8_t i2c_cmd_[3] = {0xAA, 0x00, 0x00};  // command to be sent
  float last_pressure_;
  float last_temperature_;
};

}  // namespace honeywellabp2_i2c
}  // namespace esphome
