#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace mbus {

class MBusDecoder {
 public:
  static int16_t decode_int16(const uint8_t data[2]);
  static int64_t decode_int(const std::vector<uint8_t> &data);
  static uint32_t decode_bcd_uint32(const uint8_t bcd_data[4]);
  static int64_t decode_bcd_int(const std::vector<uint8_t> &data);
  static std::string decode_manufacturer(const uint8_t data[2]);
  static std::string decode_medium(uint8_t data);
  static uint64_t decode_secondary_address(const uint8_t id[4], const uint8_t manufacturer[2], uint8_t version,
                                           uint8_t medium);
  static void encode_secondary_address(uint64_t secondary_address, std::vector<uint8_t> *data);
};

}  // namespace mbus
}  // namespace esphome
