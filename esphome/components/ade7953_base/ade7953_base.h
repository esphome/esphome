#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#include <vector>

namespace esphome {
namespace ade7953_base {

class ADE7953 : public PollingComponent, public sensor::Sensor {
 public:
  void set_irq_pin(InternalGPIOPin *irq_pin) { irq_pin_ = irq_pin; }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }

  void set_power_factor_a_sensor(sensor::Sensor *power_factor_a) { power_factor_a_sensor_ = power_factor_a; }
  void set_power_factor_b_sensor(sensor::Sensor *power_factor_b) { power_factor_b_sensor_ = power_factor_b; }

  void set_current_a_sensor(sensor::Sensor *current_a_sensor) { current_a_sensor_ = current_a_sensor; }
  void set_current_b_sensor(sensor::Sensor *current_b_sensor) { current_b_sensor_ = current_b_sensor; }

  void set_apparent_power_a_sensor(sensor::Sensor *apparent_power_a) { apparent_power_a_sensor_ = apparent_power_a; }
  void set_apparent_power_b_sensor(sensor::Sensor *apparent_power_b) { apparent_power_b_sensor_ = apparent_power_b; }

  void set_active_power_a_sensor(sensor::Sensor *active_power_a_sensor) {
    active_power_a_sensor_ = active_power_a_sensor;
  }
  void set_active_power_b_sensor(sensor::Sensor *active_power_b_sensor) {
    active_power_b_sensor_ = active_power_b_sensor;
  }

  void set_reactive_power_a_sensor(sensor::Sensor *reactive_power_a) { reactive_power_a_sensor_ = reactive_power_a; }
  void set_reactive_power_b_sensor(sensor::Sensor *reactive_power_b) { reactive_power_b_sensor_ = reactive_power_b; }

  void setup() override {
    if (this->irq_pin_ != nullptr) {
      this->irq_pin_->setup();
    }

    this->set_timeout(100, [this]() {
      this->ade_write_8(0x0010, 0x04);
      this->ade_write_8(0x00FE, 0xAD);
      this->ade_write_16(0x0120, 0x0030);
      this->is_setup_ = true;
    });
  }

  void dump_config() override;

  void update() override;

  InternalGPIOPin *irq_pin_{nullptr};
  bool is_setup_{false};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_a_sensor_{nullptr};
  sensor::Sensor *current_b_sensor_{nullptr};
  sensor::Sensor *apparent_power_a_sensor_{nullptr};
  sensor::Sensor *apparent_power_b_sensor_{nullptr};
  sensor::Sensor *active_power_a_sensor_{nullptr};
  sensor::Sensor *active_power_b_sensor_{nullptr};
  sensor::Sensor *reactive_power_a_sensor_{nullptr};
  sensor::Sensor *reactive_power_b_sensor_{nullptr};
  sensor::Sensor *power_factor_a_sensor_{nullptr};
  sensor::Sensor *power_factor_b_sensor_{nullptr};

  virtual bool ade_write_8(uint16_t reg, uint8_t value) = 0;

  virtual bool ade_write_16(uint16_t reg, uint16_t value) = 0;

  virtual bool ade_write_32(uint16_t reg, uint32_t value) = 0;

  virtual bool ade_read_16(uint16_t reg, uint16_t *value) = 0;

  virtual bool ade_read_32(uint16_t reg, uint32_t *value) = 0;
};

}  // namespace ade7953_base
}  // namespace esphome
