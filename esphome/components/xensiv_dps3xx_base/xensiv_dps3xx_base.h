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
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  bool measure_now();

 protected:
  virtual bool read_byte(uint8_t reg, uint8_t *data) = 0;
  virtual bool read_bytes(uint8_t reg, uint8_t *data, size_t len) = 0;
  virtual bool write_byte(uint8_t reg, uint8_t value) = 0;

  InternalGPIOPin *interrupt_pin_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};
  Dps3xx Dps3xxPressureSensor;

  volatile bool data_ready_{false};

  std::string failure_reason_;
};

}  // namespace xensiv_dps3xx_base
}  // namespace esphome
