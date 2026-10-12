#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct PioneerData {
  uint16_t rc_code_1;
  uint16_t rc_code_2;

  bool operator==(const PioneerData &rhs) const { return rc_code_1 == rhs.rc_code_1 && rc_code_2 == rhs.rc_code_2; }
};

class PioneerProtocol : public RemoteProtocol<PioneerData> {
 public:
  void encode(RemoteTransmitData *dst, const PioneerData &data);
  optional<PioneerData> decode(RemoteReceiveData src);
  void dump(const PioneerData &data);
};

DECLARE_REMOTE_PROTOCOL(Pioneer)

}  // namespace esphome::remote_base
