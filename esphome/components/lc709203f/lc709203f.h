#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace lc709203f {

enum LC709203FState {
  STATE_INIT,
  STATE_RSOC,
  STATE_TEMP_SETUP,
  STATE_NORMAL,
};

/// Enum listing allowable voltage settings for the LC709203F.
enum LC709203FBatteryVoltage {
  LC709203F_BATTERY_VOLTAGE_3_8 = 0x0000,
  LC709203F_BATTERY_VOLTAGE_3_7 = 0x0001,
};

class Lc709203f : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_pack_size(uint16_t pack_size);
  void set_thermistor_b_constant(uint16_t b_constant);
  void set_pack_voltage(LC709203FBatteryVoltage pack_voltage);
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_battery_remaining_sensor(sensor::Sensor *battery_remaining_sensor) {
    battery_remaining_sensor_ = battery_remaining_sensor;
  }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }

 private:
  uint8_t get_register_(uint8_t register_to_read, uint16_t *register_value);
  uint8_t set_register_(uint8_t register_to_set, uint16_t value_to_set);

 protected:
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *battery_remaining_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  uint16_t pack_size_;
  uint16_t apa_;
  uint16_t b_constant_;
  LC709203FState state_ = STATE_INIT;
  uint16_t pack_voltage_;
};

}  // namespace lc709203f
}  // namespace esphome
