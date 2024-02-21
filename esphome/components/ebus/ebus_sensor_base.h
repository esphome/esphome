#pragma once

#include "ebus_component.h"
#include <vector>

namespace esphome {
namespace ebus {

class EbusSensorBase : public EbusReceiver, public EbusSender, public Component {
 public:
  void dump_config() override;

  void set_send_poll(bool /*send_poll*/);
  void set_command(uint16_t /*command*/);
  void set_payload(const std::vector<uint8_t> & /*payload*/);

  void set_response_read_position(uint8_t /*response_position*/);

  optional<SendCommand> prepare_command() override;

  // TODO: refactor these
  uint32_t get_response_bytes(Telegram &telegram, uint8_t start, uint8_t length);
  bool is_mine(Telegram &telegram);

 protected:
  bool send_poll_;
  uint16_t command_;
  std::vector<uint8_t> payload_{};
  uint8_t response_position_;
};

}  // namespace ebus
}  // namespace esphome
