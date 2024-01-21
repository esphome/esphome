#pragma once

#include <deque>
#include <memory>
#include <vector>

#include <cstdint>

#include "mbus_frame.h"
#include "esphome/components/mbus/network_adapter.h"

namespace esphome {
namespace mbus {

class MBus;
class MBusCommand;

class MBusProtocolHandler {
 public:
  static const uint32_t RX_TIMEOUT{1000};

  MBusProtocolHandler(MBus *mbus, INetworkAdapter *network_adapter) : network_adapter_(network_adapter), mbus_(mbus) {}

  void loop();
  void register_command(MBusFrame &command, void (*response_handler)(MBusCommand *command, const MBusFrame &response),
                        uint8_t step, uint32_t delay = 0, bool wait_for_response = true);

 protected:
  // Communication
  int8_t receive_();
  int8_t send_(MBusFrame &frame);

  // Parsing
  std::unique_ptr<MBusFrame> parse_response_();
  std::unique_ptr<MBusDataVariable> parse_variable_data_response_(std::vector<uint8_t> data);
  int8_t get_dif_datalength_(uint8_t dif, std::vector<uint8_t>::iterator &it);

  // Helper
  void delete_first_command_();

  INetworkAdapter *network_adapter_{nullptr};
  MBus *mbus_{nullptr};
  std::vector<uint8_t> rx_buffer_;
  std::deque<MBusCommand *> commands_;

  uint32_t timestamp_{0};
  bool waiting_for_response_{false};
};

class MBusCommand {
 public:
  MBusFrame *command{nullptr};
  MBus *mbus{nullptr};
  uint8_t step{0};
  uint32_t created{0};
  uint32_t delay{0};
  bool wait_for_response{false};

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
