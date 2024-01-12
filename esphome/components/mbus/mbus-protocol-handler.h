#pragma once

#include <deque>
#include <memory>
#include <vector>

#include "stdint.h"

#include "mbus-frame.h"
#include "network-adapter.h"

namespace esphome {
namespace mbus {

class MBus;
class MBusCommand;
class MBusProtocolHandler {
 public:
  static const uint32_t rx_timeout{1000};

  void loop();
  void register_command(MBusFrame &command, void (*response_handler)(MBusCommand *command, const MBusFrame &response),
                        uint8_t step, uint32_t delay = 0, bool wait_for_response = true);

  MBusProtocolHandler(MBus *mbus, INetworkAdapter *networkAdapter) : _mbus(mbus), _networkAdapter(networkAdapter) {}

 protected:
  // Communication
  int8_t receive();
  int8_t send(MBusFrame &frame);

  // Parsing
  std::unique_ptr<MBusFrame> parse_response();
  std::unique_ptr<MBusDataVariable> parse_variable_data_response(std::vector<uint8_t> data);
  int8_t get_dif_datalength(const uint8_t dif, std::vector<uint8_t>::iterator &it);

  // Helper
  void delete_first_command();

  INetworkAdapter *_networkAdapter{nullptr};
  MBus *_mbus{nullptr};
  std::vector<uint8_t> _rx_buffer;
  std::deque<MBusCommand *> _commands;

  uint32_t _timestamp{0};
  bool _waiting_for_response{false};
};

class MBusCommand {
 public:
  MBusFrame *command{nullptr};
  MBus *mbus{nullptr};
  uint8_t step{0};
  uint32_t created;
  uint32_t delay;
  bool wait_for_response;

  void (*response_handler)(MBusCommand *command, const MBusFrame &response){nullptr};

  MBusCommand(MBusFrame &command, void (*response_handler)(MBusCommand *command, const MBusFrame &response),
              uint8_t step, MBus *mbus, uint32_t delay, bool wait_for_response) {
    this->command = new MBusFrame(command);
    this->created = millis();
    this->step = step;
    this->delay = delay;
    this->mbus = mbus;
    this->response_handler = response_handler;
    this->wait_for_response = wait_for_response;
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
