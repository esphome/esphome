#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace axp2101 {

class AXP2101Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void dump_config() override;
  void update() override;

  void set_temperature_sensor(sensor::Sensor *s) { temperature_sensor_ = s; }
  void set_battery_remaining_sensor(sensor::Sensor *s) { battery_remaining_sensor_ = s; }
  void set_battery_voltage_sensor(sensor::Sensor *s) { battery_voltage_sensor_ = s; }
  void set_battery_present_sensor(binary_sensor::BinarySensor *s) { battery_present_sensor_ = s; }
  void set_battery_charging_sensor(binary_sensor::BinarySensor *s) { battery_charging_sensor_ = s; }
  void set_battery_status_sensor(text_sensor::TextSensor *s) { battery_status_sensor_ = s; }

  void shutdown();

 protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *battery_remaining_sensor_{nullptr};
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  binary_sensor::BinarySensor *battery_present_sensor_{nullptr};
  binary_sensor::BinarySensor *battery_charging_sensor_{nullptr};
  text_sensor::TextSensor *battery_status_sensor_{nullptr};

  void getTemperature(void);
  void getBatteryPercentage(void);
  void getBatteryVoltage(void);
  void getStatus(void);
};

}  // namespace axp2101
}  // namespace esphome
