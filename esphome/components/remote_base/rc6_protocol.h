#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct RC6Data {
  uint8_t mode : 3;
  uint8_t toggle : 1;
  uint8_t address;
  uint8_t command;

  bool operator==(const RC6Data &rhs) const { return address == rhs.address && command == rhs.command; }
};

class RC6Protocol : public RemoteProtocol<RC6Data> {
 public:
  void encode(RemoteTransmitData *dst, const RC6Data &data);
  optional<RC6Data> decode(RemoteReceiveData src);
  void dump(const RC6Data &data);
};

DECLARE_REMOTE_PROTOCOL(RC6)

}  // namespace esphome::remote_base
