#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ina3221 {

class INA3221Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_bus_voltage_sensor(int channel, sensor::Sensor *obj) { this->channels_[channel].bus_voltage_sensor_ = obj; }
  void set_shunt_voltage_sensor(int channel, sensor::Sensor *obj) {
    this->channels_[channel].shunt_voltage_sensor_ = obj;
  }
  void set_current_sensor(int channel, sensor::Sensor *obj) { this->channels_[channel].current_sensor_ = obj; }
  void set_power_sensor(int channel, sensor::Sensor *obj) { this->channels_[channel].power_sensor_ = obj; }
  void set_shunt_resistance(int channel, float resistance_ohm);

 protected:
  struct INA3221Channel {
    float shunt_resistance_{0.1f};
    sensor::Sensor *bus_voltage_sensor_{nullptr};
    sensor::Sensor *shunt_voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *power_sensor_{nullptr};

    bool exists();
    bool should_measure_shunt_voltage();
    bool should_measure_bus_voltage();
  } channels_[3];
};

}  // namespace ina3221
}  // namespace esphome
