#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include <cmath>

namespace esphome::ina3221 {

enum INA3221Averaging {
  INA3221_AVERAGING_1 = 0b000,
  INA3221_AVERAGING_4 = 0b001,
  INA3221_AVERAGING_16 = 0b010,
  INA3221_AVERAGING_64 = 0b011,
  INA3221_AVERAGING_128 = 0b100,
  INA3221_AVERAGING_256 = 0b101,
  INA3221_AVERAGING_512 = 0b110,
  INA3221_AVERAGING_1024 = 0b111,
};

enum INA3221ConversionTime {
  INA3221_CONVERSION_TIME_140US = 0b000,
  INA3221_CONVERSION_TIME_204US = 0b001,
  INA3221_CONVERSION_TIME_332US = 0b010,
  INA3221_CONVERSION_TIME_588US = 0b011,
  INA3221_CONVERSION_TIME_1100US = 0b100,
  INA3221_CONVERSION_TIME_2116US = 0b101,
  INA3221_CONVERSION_TIME_4156US = 0b110,
  INA3221_CONVERSION_TIME_8244US = 0b111,
};

class INA3221Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_bus_voltage_sensor(int channel, sensor::Sensor *obj) { this->channels_[channel].bus_voltage_sensor_ = obj; }
  void set_shunt_voltage_sensor(int channel, sensor::Sensor *obj) {
    this->channels_[channel].shunt_voltage_sensor_ = obj;
  }
  void set_current_sensor(int channel, sensor::Sensor *obj) { this->channels_[channel].current_sensor_ = obj; }
  void set_power_sensor(int channel, sensor::Sensor *obj) { this->channels_[channel].power_sensor_ = obj; }
  void set_shunt_resistance(int channel, float resistance_ohm);
  void set_power_down_on_shutdown(bool power_down) { this->power_down_on_shutdown_ = power_down; }

  void set_averaging(INA3221Averaging averaging) { this->averaging_ = averaging; }
  void set_bus_conversion_time(INA3221ConversionTime ct) { this->bus_conversion_time_ = ct; }
  void set_shunt_conversion_time(INA3221ConversionTime ct) { this->shunt_conversion_time_ = ct; }

  void set_warning_current_limit(int channel, float current_a) {
    this->channels_[channel].warning_current_limit_ = current_a;
  }
  void set_critical_current_limit(int channel, float current_a) {
    this->channels_[channel].critical_current_limit_ = current_a;
  }

  void set_sum_shunt_voltage_sensor(sensor::Sensor *obj) { this->sum_shunt_voltage_sensor_ = obj; }
  void set_sum_current_sensor(sensor::Sensor *obj) { this->sum_current_sensor_ = obj; }
  void set_sum_power_sensor(sensor::Sensor *obj) { this->sum_power_sensor_ = obj; }

  void on_shutdown() override;

 protected:
  struct INA3221Channel {
    float shunt_resistance_{0.1f};
    sensor::Sensor *bus_voltage_sensor_{nullptr};
    sensor::Sensor *shunt_voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *power_sensor_{nullptr};

    float warning_current_limit_{NAN};
    float critical_current_limit_{NAN};

    bool exists();
    bool should_measure_shunt_voltage();
    bool should_measure_bus_voltage();
  } channels_[3];
  bool power_down_on_shutdown_{false};

  INA3221Averaging averaging_{INA3221_AVERAGING_1};
  INA3221ConversionTime bus_conversion_time_{INA3221_CONVERSION_TIME_1100US};
  INA3221ConversionTime shunt_conversion_time_{INA3221_CONVERSION_TIME_1100US};

  sensor::Sensor *sum_shunt_voltage_sensor_{nullptr};
  sensor::Sensor *sum_current_sensor_{nullptr};
  sensor::Sensor *sum_power_sensor_{nullptr};

  bool has_summation_();
};

}  // namespace esphome::ina3221
