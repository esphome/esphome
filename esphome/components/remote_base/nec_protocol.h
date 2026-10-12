#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct NECData {
  uint16_t address;
  uint16_t command;
  uint16_t command_repeats;

  bool operator==(const NECData &rhs) const { return address == rhs.address && command == rhs.command; }
};

class NECProtocol : public RemoteProtocol<NECData> {
 public:
  void encode(RemoteTransmitData *dst, const NECData &data);
  optional<NECData> decode(RemoteReceiveData src);
  void dump(const NECData &data);
};

DECLARE_REMOTE_PROTOCOL(NEC)

}  // namespace esphome::remote_base
