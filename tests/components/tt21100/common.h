#pragma once

#include <algorithm>
#include <array>
#include <cstring>

#include "esphome/components/tt21100/touchscreen/tt21100.h"

namespace esphome::tt21100::testing {

// Every read returns the start of a 14-byte button report, so the 2-byte length read sees 14.
class ReportBus final : public i2c::I2CBus {
 public:
  std::array<uint8_t, 14> report{14, 0, 0x03};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    if (out_size == 0)
      return i2c::ERROR_OK;
    std::memcpy(out, this->report.data(), std::min(out_size, this->report.size()));
    return i2c::ERROR_OK;
  }

  void set_signals(uint16_t s0, uint16_t s1, uint16_t s2, uint16_t s3) {
    const uint16_t signals[4] = {s0, s1, s2, s3};
    std::memcpy(&this->report[6], signals, sizeof(signals));
  }
};

}  // namespace esphome::tt21100::testing
