#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

/// Denon (Sharp-format) 15-bit IR code: the Device Code and Function # from Denon's IR code tables.
struct DenonData {
  uint8_t address;  // 5 bits
  uint8_t command;  // 8 bits

  bool operator==(const DenonData &rhs) const { return address == rhs.address && command == rhs.command; }
};

class DenonProtocol : public RemoteProtocol<DenonData> {
 public:
  void encode(RemoteTransmitData *dst, const DenonData &data);
  optional<DenonData> decode(RemoteReceiveData src);
  void dump(const DenonData &data);
};

DECLARE_REMOTE_PROTOCOL(Denon)

}  // namespace esphome::remote_base
