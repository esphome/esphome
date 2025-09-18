#pragma once

#include "esphome/core/defines.h"
#include <cstdint>
#include <string>
#include <memory>

namespace esphome {
namespace sha256 {

class SHA256 {
 public:
  SHA256() = default;
  ~SHA256();

  void init();
  void add(const uint8_t *data, size_t len);
  void add(const char *data, size_t len) { this->add((const uint8_t *) data, len); }
  void add(const std::string &data) { this->add(data.c_str(), data.length()); }

  void calculate();

  void get_bytes(uint8_t *output);
  void get_hex(char *output);
  std::string get_hex_string();

  bool equals_bytes(const uint8_t *expected);
  bool equals_hex(const char *expected);

 protected:
  struct SHA256Context;
  std::unique_ptr<SHA256Context> ctx_;
};

}  // namespace sha256
}  // namespace esphome
