#pragma once

#include <deque>
#include <memory>
#include <vector>

#include "stdint.h"

#include "mbus-frame.h"
#include "network-adapter.h"

namespace esphome {
namespace mbus {

class MBusCommand;
class MBusProtocolHandler {
 public:
  static const uint32_t rx_timeout{500};

  void loop();
  void register_command(MBusFrame &command, void (*response_handler)(MBusCommand *command, const MBusFrame &response),
                        uint8_t data = 0, uint32_t delay = 0);

  MBusProtocolHandler(INetworkAdapter *networkAdapter) : _networkAdapter(networkAdapter) {}

 protected:
  int8_t receive();
  int8_t send(MBusFrame &frame);
  std::unique_ptr<MBusFrame> parse_response();

  INetworkAdapter *_networkAdapter;
  std::vector<uint8_t> _rx_buffer;
  std::deque<MBusCommand *> _commands;

  uint32_t _timestamp{0};
  bool _waiting_for_response{false};
};

class MBusCommand {
 public:
  MBusFrame *command{nullptr};
  MBusProtocolHandler *protocol_handler{nullptr};
  uint8_t data{0};
  uint32_t create;
  uint32_t delay;

  void (*response_handler)(MBusCommand *command, const MBusFrame &response){nullptr};

  MBusCommand(MBusFrame &_command, void (*_response_handler)(MBusCommand *command, const MBusFrame &response),
              uint8_t _data, MBusProtocolHandler *_protocol_handler, uint32_t delay) {
    this->command = new MBusFrame(_command);
    this->response_handler = _response_handler;
    this->data = _data;
    this->protocol_handler = _protocol_handler;
    this->create = millis();
    this->delay = delay;
  }

  ~MBusCommand() {
    if (this->command != nullptr) {
      delete this->command;
      this->command = nullptr;
    }
  }
};

}  // namespace mbus
}  // namespace esphome
