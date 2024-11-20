#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include <vector>
#include <map>

namespace esphome {
namespace duco {

class DucoDevice;

class DucoMessage {
 public:
  DucoMessage() {}

  uint8_t function;
  uint8_t id;
  std::vector<uint8_t> data;
  uint16_t crc;

  std::vector<uint8_t> get_message() {
    std::vector<uint8_t> message;
    message.push_back(data.size() + 2);
    message.push_back(function);
    message.push_back(id);
    message.insert(message.end(), data.begin(), data.end());

    auto crc = crc16(message.data(), message.size());

    message.push_back(crc & 0xFF);
    message.push_back((crc >> 8) & 0xFF);

    return message;
  }

  std::string to_string() {
    std::string res;

    char buf[5];

    size_t len = data.size();

    sprintf(buf, "%02X", len + 2);
    res += buf;
    res += "  ";

    sprintf(buf, "%02X", function);
    res += buf;
    res += " ";

    sprintf(buf, "%02X", id);
    res += buf;
    res += " ";

    for (size_t i = 0; i < len; i++) {
      res += " ";
      sprintf(buf, "%02X", data[i]);
      res += buf;
    }

    return res;
  }
};

class Duco : public uart::UARTDevice, public Component {
 public:
  Duco() { this->last_id_ = 10; };

  void setup() override;

  void loop() override;

  void dump_config() override;

  float get_setup_priority() const override;

  void send(DucoMessage message, DucoDevice *device);
  void set_send_wait_time(uint16_t time_in_ms) { send_wait_time_ = time_in_ms; }
  void set_disable_crc(bool disable_crc) { disable_crc_ = disable_crc; }

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
  virtual void receive_response(DucoMessage message) {}
};

class DucoDiscovery : public DucoDevice, public PollingComponent {
 public:
  void loop() override;
  void update() override;

  void receive_response(DucoMessage message) override;

  static const std::string NODE_TYPE_UCBAT;
  static const std::string NODE_TYPE_UC;
  static const std::string NODE_TYPE_UCCO2;
  static const std::string NODE_TYPE_BOX;
  static const std::string NODE_TYPE_SWITCH;
  static const std::string NODE_TYPE_UNKNOWN;

  static const uint8_t NODE_TYPE_CODE_UCBAT = 8;
  static const uint8_t NODE_TYPE_CODE_UC = 9;
  static const uint8_t NODE_TYPE_CODE_UCCO2 = 12;
  static const uint8_t NODE_TYPE_CODE_BOX = 17;
  static const uint8_t NODE_TYPE_CODE_SWITCH = 18;

 protected:
  // start with a delay of 1000 loops
  uint32_t delay_{1000};
  uint8_t next_node_{0};
  bool waiting_for_response_ = false;

  std::vector<std::tuple<uint8_t, uint8_t>> nodes_;
};

}  // namespace duco
}  // namespace esphome
