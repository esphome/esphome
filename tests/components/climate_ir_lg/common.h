#pragma once

#include <cstdint>
#include <vector>
#include "esphome/components/climate_ir_lg/climate_ir_lg.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome::climate_ir_lg::testing {

// Records the raw mark/space timings instead of driving real IR hardware.
class MockRemoteTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  MockRemoteTransmitter() : RemoteTransmitterBase(nullptr) {}

  std::vector<int32_t> last_data;

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override { this->last_data = this->temp_.get_data(); }
};

// Decodes the raw mark/space timings produced by LgIrClimate::transmit_() back into the
// 28-bit value that was transmitted (header + bits, ignoring the trailing mark).
inline uint32_t decode_lg_frame(const std::vector<int32_t> &data, uint32_t bit_one_low) {
  uint32_t value = 0;
  // data[0]/data[1] are the header mark/space; each bit thereafter is a mark/space pair.
  for (size_t i = 0; i < 28; i++) {
    int32_t space = data[2 + 2 * i + 1];
    value = (value << 1) | (static_cast<uint32_t>(-space) == bit_one_low ? 1 : 0);
  }
  return value;
}

}  // namespace esphome::climate_ir_lg::testing
