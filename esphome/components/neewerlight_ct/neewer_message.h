#pragma once

#include <cstdint>
#include <vector>

#ifdef USE_ESP32

namespace esphome::neewerlight_ct {

enum class MessageType : uint8_t {
  UNDEFINED = 0x00,
  COMMAND = 0x78,
};

enum class CommandType : uint8_t {
  UNDEFINED = 0x00,
  TURN_ON_OFF = 0x81,
  CT_BRIGHTNESS = 0x87,
};

struct Message {
  MessageType msg_type = MessageType::UNDEFINED;
  CommandType cmd_type = CommandType::UNDEFINED;
  std::vector<uint8_t> payload = {};
};

}  // namespace esphome::neewerlight_ct

#endif  // USE_ESP32
