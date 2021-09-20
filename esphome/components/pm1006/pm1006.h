#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace pm1006 {

class PM1006Component : public PollingComponent, public uart::UARTDevice {
 public:
  PM1006Component() = default;

  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor) { this->pm_2_5_sensor_ = pm_2_5_sensor; }
  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

  float get_setup_priority() const override;

 protected:
  optional<bool> check_byte_() const;
  void parse_data_();
  uint16_t get_16_bit_uint_(uint8_t start_index) const;
  uint8_t pm1006_checksum_(const uint8_t *command_data, uint8_t length) const;

  sensor::Sensor *pm_2_5_sensor_{nullptr};

  uint8_t data_[20];
  uint8_t data_index_{0};
  uint32_t last_transmission_{0};
};

}  // namespace pm1006
}  // namespace esphome
