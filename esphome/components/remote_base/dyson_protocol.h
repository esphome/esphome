#pragma once

#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

static constexpr uint8_t IGNORE_INDEX = 0xFF;

struct DysonData {
  uint16_t code;  // the button, e.g. power, swing, fan++, ...
  uint8_t index;  // the rolling index counter
  bool operator==(const DysonData &rhs) const {
    if (IGNORE_INDEX == index || IGNORE_INDEX == rhs.index) {
      return code == rhs.code;
    }
    return code == rhs.code && index == rhs.index;
  }
};

class DysonProtocol : public RemoteProtocol<DysonData> {
 public:
  void encode(RemoteTransmitData *dst, const DysonData &data);
  optional<DysonData> decode(RemoteReceiveData src);
  void dump(const DysonData &data);
};

DECLARE_REMOTE_PROTOCOL(Dyson)

}  // namespace esphome::remote_base
