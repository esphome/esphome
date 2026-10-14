#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct SymphonyData {
  uint32_t data;
  uint8_t nbits;
  uint8_t repeats{1};

  bool operator==(const SymphonyData &rhs) const { return data == rhs.data && nbits == rhs.nbits; }
};

class SymphonyProtocol : public RemoteProtocol<SymphonyData> {
 public:
  void encode(RemoteTransmitData *dst, const SymphonyData &data);
  optional<SymphonyData> decode(RemoteReceiveData src);
  void dump(const SymphonyData &data);
};

DECLARE_REMOTE_PROTOCOL(Symphony)

}  // namespace esphome::remote_base
