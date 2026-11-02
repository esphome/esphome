#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

using TotoData = uint16_t;

class TotoProtocol : public RemoteProtocol<TotoData> {
 public:
  void encode(RemoteTransmitData *dst, const TotoData &code);
  optional<TotoData> decode(RemoteReceiveData src);
  void dump(const TotoData &code);
};

DECLARE_REMOTE_PROTOCOL(Toto)

}  // namespace esphome::remote_base
