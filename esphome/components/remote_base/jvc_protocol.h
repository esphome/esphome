#pragma once

#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct JVCData {
  uint32_t data;

  bool operator==(const JVCData &rhs) const { return data == rhs.data; }
};

class JVCProtocol : public RemoteProtocol<JVCData> {
 public:
  void encode(RemoteTransmitData *dst, const JVCData &data);
  optional<JVCData> decode(RemoteReceiveData src);
  void dump(const JVCData &data);
};

DECLARE_REMOTE_PROTOCOL(JVC)

}  // namespace esphome::remote_base
