#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include <cstddef>
#include <vector>

namespace esphome::vbus {

using message_parser_t = std::function<float(std::vector<uint8_t> &)>;

// Payload offsets are fixed per model; a short message means the controller is not the configured
// model or the telegram was truncated. Listeners guard each field with this so a telegram missing
// only the tail fields still publishes the ones that fit.
inline bool fits(const std::vector<uint8_t> &message, size_t start, size_t width) {
  return message.size() >= start + width;
}

class VBus;

class VBusListener {
 public:
  void set_command(uint16_t command) { this->command_ = command; }
  void set_source(uint16_t source) { this->source_ = source; }
  void set_dest(uint16_t dest) { this->dest_ = dest; }

  void on_message(uint16_t command, uint16_t source, uint16_t dest, std::vector<uint8_t> &message);

 protected:
  uint16_t command_{0xffff};
  uint16_t source_{0xffff};
  uint16_t dest_{0xffff};

  virtual void handle_message(std::vector<uint8_t> &message) = 0;
};

class VBus final : public uart::UARTDevice, public Component {
 public:
  void dump_config() override;
  void loop() override;

  void register_listener(VBusListener *listener) { this->listeners_.push_back(listener); }

 protected:
  // A telegram is <sync 0xaa> <9 or 15 byte header> [<6 byte frame> ...]; every other byte is 7-bit,
  // so a byte with bit 7 set that is not the sync byte always aborts the telegram in progress.
  enum class ParseState : uint8_t { PARSE_STATE_IDLE, PARSE_STATE_HEADER, PARSE_STATE_FRAMES };

  ParseState state_{ParseState::PARSE_STATE_IDLE};
  std::vector<uint8_t> buffer_;
  uint8_t protocol_{0};
  uint16_t source_{0};
  uint16_t dest_{0};
  uint16_t command_{0};
  uint8_t frames_{0};
  uint8_t cframe_{0};
  uint8_t fbytes_[6]{};
  uint8_t fbcount_{0};
  std::vector<VBusListener *> listeners_{};
};

}  // namespace esphome::vbus
