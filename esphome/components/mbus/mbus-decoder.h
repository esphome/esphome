#pragma once
#include "stdint.h"
#include <string>
#include <vector>

namespace esphome {
namespace mbus {

class MBusDecoder {
 public:
  static int16_t decode_int(uint8_t data[2]);
  static std::string decode_manufacturer(uint8_t data[2]);
  static std::string decode_medium(uint8_t data);
  static uint32_t decode_bcd_hex(uint8_t bcd_data[4]);
};

}  // namespace mbus
}  // namespace esphome
