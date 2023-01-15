#pragma once

#ifdef USE_ESP32

#include <string>
#include <vector>

namespace esphome {
namespace motion_blinds {

class Crypto {
 public:
  static std::string encrypt(const std::string &data);
  static std::string decrypt(const std::string &data);
};

}  // namespace motion_blinds
}  // namespace esphome

#endif  // USE_ESP32
