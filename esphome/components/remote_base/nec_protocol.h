#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct NECData {
  uint16_t address;
  uint16_t command;
  uint16_t command_repeats;

  bool operator==(const NECData &rhs) const { return address == rhs.address && command == rhs.command; }
};

class NECProtocol : public RemoteProtocol<NECData> {
 public:
  // Signed, so they also serve as the durations of a raw frame (beken_irda)
  static constexpr int32_t HEADER_HIGH_US = 9000;
  static constexpr int32_t HEADER_LOW_US = 4500;
  static constexpr int32_t BIT_HIGH_US = 560;
  static constexpr int32_t BIT_ONE_LOW_US = 1690;
  static constexpr int32_t BIT_ZERO_LOW_US = 560;
  static constexpr int32_t REPEAT_HEADER_LOW_US = HEADER_LOW_US / 2;

  void encode(RemoteTransmitData *dst, const NECData &data);
  optional<NECData> decode(RemoteReceiveData src);
  void dump(const NECData &data);
};

DECLARE_REMOTE_PROTOCOL(NEC)

}  // namespace esphome::remote_base
