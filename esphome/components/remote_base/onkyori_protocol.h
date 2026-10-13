#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct OnkyoRIData {
  uint16_t data;

  bool operator==(const OnkyoRIData &rhs) const { return data == rhs.data; }
};

class OnkyoRIProtocol : public RemoteProtocol<OnkyoRIData> {
 public:
  void encode(RemoteTransmitData *dst, const OnkyoRIData &data);
  optional<OnkyoRIData> decode(RemoteReceiveData src);
  void dump(const OnkyoRIData &data);
};

DECLARE_REMOTE_PROTOCOL(OnkyoRI)

}  // namespace esphome::remote_base
