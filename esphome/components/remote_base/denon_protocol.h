#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

/// Denon (Sharp-format) 15-bit IR code.
///
/// address and command are the "Device Code" and "Function #" from Denon's IR code
/// tables. Each code is sent as a pair of frames: the normal frame, then the same
/// frame with the command and the two trailing bits inverted.
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
