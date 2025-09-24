#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace cse7761 {

struct CSE7761DataStruct {
  uint32_t frequency = 0;
  uint32_t voltage_rms = 0;
  uint32_t current_rms[2] = {0};
  uint32_t energy[2] = {0};
  uint32_t active_power[2] = {0};
  uint16_t coefficient[8] = {0};
  uint8_t energy_update = 0;
  bool ready = false;
};

/// This class implements support for the CSE7761 UART power sensor.
class CSE7761Component : public PollingComponent, public uart::UARTDevice {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_active_power_1_sensor(sensor::Sensor *power_sensor_1) { power_sensor_1_ = power_sensor_1; }
  void set_current_1_sensor(sensor::Sensor *current_sensor_1) { current_sensor_1_ = current_sensor_1; }
  void set_active_power_2_sensor(sensor::Sensor *power_sensor_2) { power_sensor_2_ = power_sensor_2; }
  void set_current_2_sensor(sensor::Sensor *current_sensor_2) { current_sensor_2_ = current_sensor_2; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  // Sensors
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *power_sensor_1_{nullptr};
  sensor::Sensor *current_sensor_1_{nullptr};
  sensor::Sensor *power_sensor_2_{nullptr};
  sensor::Sensor *current_sensor_2_{nullptr};
  CSE7761DataStruct data_;

  void write_(uint8_t reg, uint16_t data);
  bool read_once_(uint8_t reg, uint8_t size, uint32_t *value);
  uint32_t read_(uint8_t reg, uint8_t size);
  uint32_t coefficient_by_unit_(uint32_t unit);
  bool chip_init_();
  void get_data_();
};

}  // namespace cse7761
}  // namespace esphome
