#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::ina3221 {

class INA3221Component final : public PollingComponent, public i2c::I2CDevice {
#ifdef USE_INA3221_SUMMATION
  SUB_SENSOR(sum_shunt_voltage)
  SUB_SENSOR(sum_current)
  SUB_SENSOR(sum_power)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void on_powerdown() override;

  void set_bus_voltage_sensor(int channel, sensor::Sensor *obj) { this->channels_[channel].bus_voltage_sensor_ = obj; }
  void set_shunt_voltage_sensor(int channel, sensor::Sensor *obj) {
    this->channels_[channel].shunt_voltage_sensor_ = obj;
  }
  void set_current_sensor(int channel, sensor::Sensor *obj) { this->channels_[channel].current_sensor_ = obj; }
  void set_power_sensor(int channel, sensor::Sensor *obj) { this->channels_[channel].power_sensor_ = obj; }
  void set_shunt_resistance(int channel, float resistance_ohm);
  /// Configuration register value built by code generation: channel enables, averaging, conversion times, mode
  void set_config_register(uint16_t config) { this->config_ = config; }
  /// Time one single-shot conversion of every enabled channel takes; zero means continuous mode
  void set_single_shot_wait_ms(uint16_t wait_ms) { this->single_shot_wait_ms_ = wait_ms; }
#ifdef USE_INA3221_ALERT_LIMITS
  void set_warning_limit(int channel, uint16_t reg) { this->channels_[channel].warning_limit_ = reg; }
  void set_critical_limit(int channel, uint16_t reg) { this->channels_[channel].critical_limit_ = reg; }
#endif

 protected:
  void read_data_();

  struct INA3221Channel {
    float shunt_resistance_{0.1f};
    sensor::Sensor *bus_voltage_sensor_{nullptr};
    sensor::Sensor *shunt_voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *power_sensor_{nullptr};
#ifdef USE_INA3221_ALERT_LIMITS
    // Alert limit register values; zero keeps the chip default, which never trips
    uint16_t warning_limit_{0};
    uint16_t critical_limit_{0};
#endif

    bool should_measure_shunt_voltage();
    bool should_measure_bus_voltage();
  } channels_[3];
  uint16_t config_{0};
  uint16_t single_shot_wait_ms_{0};
};

}  // namespace esphome::ina3221
