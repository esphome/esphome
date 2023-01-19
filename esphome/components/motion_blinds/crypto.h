#pragma once

#ifdef USE_ESP32

#include <string>
#include <vector>

namespace esphome {
namespace motion_blinds {

struct MotionBlindsMessage {
  size_t length;
  uint8_t bytes[128];
  std::string raw_command;
};

class Crypto {
 public:
  static void encrypt(const std::string &data, MotionBlindsMessage &message);
  static std::string decrypt(const uint8_t *data, size_t size);
};

}  // namespace motion_blinds
}  // namespace esphome

#endif  // USE_ESP32
