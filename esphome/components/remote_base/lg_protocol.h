#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct LGData {
  uint32_t data;
  uint8_t nbits;

  bool operator==(const LGData &rhs) const { return data == rhs.data && nbits == rhs.nbits; }
};

class LGProtocol : public RemoteProtocol<LGData> {
 public:
  void encode(RemoteTransmitData *dst, const LGData &data);
  optional<LGData> decode(RemoteReceiveData src);
  void dump(const LGData &data);
};

DECLARE_REMOTE_PROTOCOL(LG)

}  // namespace esphome::remote_base
