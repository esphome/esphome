#pragma once

#include <vector>

#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/helpers.h"

namespace esphome::scd4x::testing {

// Records every write and answers every read with zero words carrying a valid CRC
class SensirionBus final : public i2c::I2CBus {
 public:
  std::vector<std::vector<uint8_t>> writes;

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *out, size_t out_size, uint8_t *in,
                             size_t in_size) override {
    if (out_size > 0)
      this->writes.emplace_back(out, out + out_size);
    static constexpr uint8_t ZERO_WORD[2]{};
    for (size_t i = 0; i < in_size; i++)
      in[i] = i % 3 == 2 ? crc8(ZERO_WORD, 2, 0xFF, 0x31, true) : 0;
    return i2c::ERROR_OK;
  }

  // Data words of every write of `command`
  std::vector<uint16_t> words_for(uint16_t command) const {
    std::vector<uint16_t> words;
    for (const auto &w : this->writes) {
      if (w.size() >= 5 && encode_uint16(w[0], w[1]) == command)
        words.push_back(encode_uint16(w[2], w[3]));
    }
    return words;
  }
};

}  // namespace esphome::scd4x::testing
