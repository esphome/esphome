#pragma once
#include "types.h"

namespace esphome {
namespace mr24hpb1 {

class Packet {
 public:
  Packet() = default;
  Packet(uint16_t size);
  ~Packet() = default;

  void clear();

  void log();
  FunctionCode function_code();
  AddressCode1 address_code_1();
  AddressCode2 address_code_2();
  uint16_t length();
  uint16_t crc();
  uint32_t data_as_int();
  std::vector<uint8_t> data();
  std::string data_as_string();
  float data_as_float();
  uint8_t *raw_data();
  std::vector<uint8_t> &raw_vect();
  uint16_t raw_size();

  void append_data(std::vector<uint8_t> &data);
  void push_data(uint8_t data);

 protected:
  std::vector<uint8_t> packet_data_;
};
}  // namespace mr24hpb1
}  // namespace esphome
