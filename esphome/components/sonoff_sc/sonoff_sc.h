#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace sonoff_sc {

class SonoffSCComponent : public Component, public uart::UARTDevice {
 public:
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_light_sensor(sensor::Sensor *light_sensor) { light_sensor_ = light_sensor; }
  void set_noise_sensor(sensor::Sensor *noise_sensor) { noise_sensor_ = noise_sensor; }
  void set_dust_sensor(sensor::Sensor *dust_sensor) { dust_sensor_ = dust_sensor; }
  void set_update_interval(uint32_t val) { /* ignore */
  }
  void set_update_interval_sec(uint32_t update_interval) { update_interval_sec_ = update_interval; }
  void set_humidity_threshold(uint32_t humidity_threshold) { humidity_threshold_ = humidity_threshold; }
  void set_temperature_threshold(uint32_t temperature_threshold) { temperature_threshold_ = temperature_threshold; }

  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

 protected:
  void parse_data_();
  int get_value_for_(const std::string &command, const std::string &prefix);
  void process_status_request_();

  uint8_t raw_data_[128];
  uint8_t raw_data_index_{0};
  uint32_t last_transmission_{0};
  uint32_t last_update_time_{0};
  uint32_t update_interval_sec_{0};
  uint32_t humidity_threshold_{1};
  uint32_t temperature_threshold_{1};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *light_sensor_{nullptr};
  sensor::Sensor *noise_sensor_{nullptr};
  sensor::Sensor *dust_sensor_{nullptr};
};

}  // namespace sonoff_sc
}  // namespace esphome
