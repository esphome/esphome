#pragma once
#include "stdint.h"
#include <string>
#include <vector>

namespace esphome {
namespace mbus {

class MBusDecoder {
 public:
  static int16_t decode_int(const uint8_t data[2]);
  static std::string decode_manufacturer(const uint8_t data[2]);
  static std::string decode_medium(const uint8_t data);
  static uint32_t decode_bcd_hex(const uint8_t bcd_data[4]);
  static uint64_t decode_secondary_address(const uint8_t id[4], const uint8_t manufacturer[2], const uint8_t version,
                                           const uint8_t medium);
  static void encode_secondary_address(const uint64_t secondary_address, std::vector<uint8_t> *data);
};

}  // namespace mbus
}  // namespace esphome
