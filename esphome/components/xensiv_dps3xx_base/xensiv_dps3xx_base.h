#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/gpio.h"
#include "Dps3xx.h"

namespace esphome {
namespace xensiv_dps3xx_base {

class XensivDPS3xx : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { this->pressure_sensor_ = pressure_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_operation_mode(uint8_t mode) { this->operation_mode_ = mode; }
  void set_sensor_rate_value(uint32_t sensor_rate) { this->sensor_rate_value_ = sensor_rate; }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  bool measure_temperature_now();
  bool measure_pressure_now();

  // Allow DpsClass to access protected I2C methods
  friend class DpsClass;
  friend class Dps3xx;

 protected:
  virtual bool read_byte(uint8_t reg, uint8_t *data) = 0;
  virtual bool read_bytes(uint8_t reg, uint8_t *data, size_t len) = 0;
  virtual bool write_byte(uint8_t reg, uint8_t value) = 0;
  static void gpio_intr(XensivDPS3xx *arg);

  InternalGPIOPin *interrupt_pin_{nullptr};
  uint8_t operation_mode_{0};
  uint8_t sensor_rate_value_{0};
  sensor::Sensor *pressure_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  Dps3xx *Dps3xxPressureSensor{nullptr};

  volatile bool data_ready_{false};

  std::string failure_reason_;
};

}  // namespace xensiv_dps3xx_base
}  // namespace esphome
