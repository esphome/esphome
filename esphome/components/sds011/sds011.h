#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace sds011 {

class SDS011Component : public Component, public uart::UARTDevice {
 public:
  SDS011Component() = default;

  /// Manually set the rx-only mode. Defaults to false.
  void set_rx_mode_only(bool rx_mode_only);

  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5_sensor) { pm_2_5_sensor_ = pm_2_5_sensor; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0_sensor) { pm_10_0_sensor_ = pm_10_0_sensor; }
  void setup() override;
  void dump_config() override;
  void loop() override;

  float get_setup_priority() const override;

  void set_update_interval(uint32_t val) { /* ignore */
  }
  void set_update_interval_min(uint8_t update_interval_min);
  void set_working_state(bool working_state);

 protected:
  void sds011_write_command_(const uint8_t *command);
  uint8_t sds011_checksum_(const uint8_t *command_data, uint8_t length) const;
  optional<bool> check_byte_() const;
  void parse_data_();
  uint16_t get_16_bit_uint_(uint8_t start_index) const;

  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};

  uint8_t data_[10];
  uint8_t data_index_{0};
  uint32_t last_transmission_{0};
  uint8_t update_interval_min_;

  bool rx_mode_only_;
};

}  // namespace sds011
}  // namespace esphome
