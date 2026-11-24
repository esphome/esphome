#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct SonyData {
  uint32_t data;
  uint8_t nbits;

  bool operator==(const SonyData &rhs) const { return data == rhs.data && nbits == rhs.nbits; }
};

class SonyProtocol : public RemoteProtocol<SonyData> {
 public:
  void encode(RemoteTransmitData *dst, const SonyData &data);
  optional<SonyData> decode(RemoteReceiveData src);
  void dump(const SonyData &data);
};

DECLARE_REMOTE_PROTOCOL(Sony)

}  // namespace esphome::remote_base
