#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include <vector>
#include <map>

namespace esphome {
namespace duco {

class DucoDevice;

class Duco : public uart::UARTDevice, public Component {
 public:
  Duco() { this->last_id_ = 10; };

  void setup() override;

  void loop() override;

  void dump_config() override;

  float get_setup_priority() const override;

  void send(uint8_t function, std::vector<uint8_t> message, DucoDevice *device);
  void send_raw(const std::vector<uint8_t> &payload);
  void set_send_wait_time(uint16_t time_in_ms) { send_wait_time_ = time_in_ms; }
  void set_disable_crc(bool disable_crc) { disable_crc_ = disable_crc; }

  void add_sensor_item(DucoDevice *sensor);

  std::map<uint8_t, DucoDevice *> waiting_for_response;
  void stop_waiting(uint8_t message_id);

 protected:
  uint8_t last_id_ = 0;
  uint8_t next_id_();

  bool parse_byte_(uint8_t byte);
  void finalize_message_();
  uint16_t send_wait_time_{250};
  bool disable_crc_;
  std::vector<uint8_t> rx_buffer_;
  uint32_t last_byte_{0};
  uint32_t last_send_{0};
  bool removed_last_ = false;

  void debug_hex(std::vector<uint8_t> bytes, uint8_t separator);
};

class DucoDevice : public Parented<Duco> {
 public:
  virtual void receive_response(std::vector<uint8_t> message) {}
};

}  // namespace duco
}  // namespace esphome
