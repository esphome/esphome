#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct Samsung36Data {
  uint16_t address;
  uint32_t command;

  bool operator==(const Samsung36Data &rhs) const { return address == rhs.address && command == rhs.command; }
};

class Samsung36Protocol : public RemoteProtocol<Samsung36Data> {
 public:
  void encode(RemoteTransmitData *dst, const Samsung36Data &data);
  optional<Samsung36Data> decode(RemoteReceiveData src);
  void dump(const Samsung36Data &data);
};

DECLARE_REMOTE_PROTOCOL(Samsung36)

}  // namespace esphome::remote_base
