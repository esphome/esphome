#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome::remote_base {

struct RC5Data {
  uint8_t address;
  uint8_t command;

  bool operator==(const RC5Data &rhs) const { return address == rhs.address && command == rhs.command; }
};

class RC5Protocol : public RemoteProtocol<RC5Data> {
 public:
  void encode(RemoteTransmitData *dst, const RC5Data &data);
  optional<RC5Data> decode(RemoteReceiveData src);
  void dump(const RC5Data &data);
};

DECLARE_REMOTE_PROTOCOL(RC5)

}  // namespace esphome::remote_base
