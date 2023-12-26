#pragma once

#include <vector>
#include <deque>
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
                        uint8_t data = 0);

  MBusProtocolHandler(INetworkAdapter *networkAdapter) : _networkAdapter(networkAdapter) {}

 protected:
  int8_t receive();
  int8_t send(MBusFrame &frame);
  MBusFrame *parse();

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
  void (*response_handler)(MBusCommand *command, const MBusFrame &response){nullptr};

  MBusCommand(MBusFrame &_command, void (*_response_handler)(MBusCommand *command, const MBusFrame &response),
              uint8_t _data, MBusProtocolHandler *_protocol_handler) {
    this->command = new MBusFrame(_command);
    this->response_handler = _response_handler;
    this->data = _data;
    this->protocol_handler = _protocol_handler;
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
