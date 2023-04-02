#pragma once

#include <array>
#include <map>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace kamstrup {

class Kamstrup : public PollingComponent, public uart::UARTDevice {
 public:
  void set_sensor(uint16_t reg, sensor::Sensor *sensor) {
    this->sensors_[reg] = sensor;
  }
  void set_receive_timeout(uint32_t seconds) { this->receive_timeout_ = seconds; }
  void set_bundle_requests(int count) { this->bundle_requests_ = count; }

  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override;
  void loop() override;
  void update() override;

  void dump_config() override;

 protected:
  void handle_serial_();
  void send_command_(const std::vector<uint16_t>& regs);
  int consume_register_(const uint8_t *msg, const uint8_t *end, uint16_t* register_id, float* value);
  uint16_t crc_1021_(const uint8_t msg[], size_t msgsize) const;

  std::map<uint16_t, sensor::Sensor *> sensors_;
  uint32_t receive_timeout_{2000};
  uint8_t bundle_requests_{1};

  std::vector<uint16_t> queue_;
  std::vector<uint16_t> working_regs_;
  enum { IDLE, START, RETRY, WAIT_BEGIN, DATA, ESCAPED_DATA, LINE } state_{IDLE};
  int retry_{0};
  int bufsize_{0};
  int32_t starttime_{0};
  std::array<uint8_t, 512> buffer_;
};

} // namespace kamstrup
} // namespace esphome

// vim: set expandtab tabstop=2 shiftwidth=2:
