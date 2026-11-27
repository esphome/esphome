#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome::remote_base {

struct SamsungData {
  uint64_t data;
  uint8_t nbits;

  bool operator==(const SamsungData &rhs) const { return data == rhs.data && nbits == rhs.nbits; }
};

class SamsungProtocol : public RemoteProtocol<SamsungData> {
 public:
  void encode(RemoteTransmitData *dst, const SamsungData &data);
  optional<SamsungData> decode(RemoteReceiveData src);
  void dump(const SamsungData &data);
};

DECLARE_REMOTE_PROTOCOL(Samsung)

}  // namespace esphome::remote_base
