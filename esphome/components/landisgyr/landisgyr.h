#pragma once

#include <string>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace landisgyr {

enum class State { IDLE, RECIEVE_INIT, RECEIVE_VALUES };

class LandisSensor : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void loop() override;
  void update() override;
  void dump_config() override;

  void set_kwh_sensor(sensor::Sensor *s) { kwh_sensor_ = s; }
  void set_volume_sensor(sensor::Sensor *s) { volume_sensor_ = s; }

 protected:
  sensor::Sensor *kwh_sensor_{nullptr};
  sensor::Sensor *volume_sensor_{nullptr};

  State state_ = State::IDLE;

  std::string read_line_();
  void parse_first_line_(const std::string &line);
  std::string parse_delimiter_(const std::string &string_to_parse, const std::string &first, const std::string &second);

  void send_request_();

  bool read_only_first_line_{true};

  std::string init_message_{"/LUGCUH50"};
  std::string buffer_string_;
  int initialized_{false};
};

}  // namespace landisgyr
}  // namespace esphome
