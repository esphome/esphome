#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome::remote_base {

struct ToshibaAcData {
  uint64_t rc_code_1;
  uint64_t rc_code_2;

  bool operator==(const ToshibaAcData &rhs) const { return rc_code_1 == rhs.rc_code_1 && rc_code_2 == rhs.rc_code_2; }
};

class ToshibaAcProtocol : public RemoteProtocol<ToshibaAcData> {
 public:
  void encode(RemoteTransmitData *dst, const ToshibaAcData &data);
  optional<ToshibaAcData> decode(RemoteReceiveData src);
  void dump(const ToshibaAcData &data);
};

DECLARE_REMOTE_PROTOCOL(ToshibaAc)

}  // namespace esphome::remote_base
