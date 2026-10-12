#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct TotoData {
  uint8_t rc_code_1 : 4;
  uint8_t rc_code_2 : 4;
  uint8_t command;

  bool operator==(const TotoData &rhs) const {
    return (rc_code_1 == rhs.rc_code_1) && (rc_code_2 == rhs.rc_code_2) && (command == rhs.command);
  }
};

class TotoProtocol : public RemoteProtocol<TotoData> {
 public:
  void encode(RemoteTransmitData *dst, const TotoData &data);
  optional<TotoData> decode(RemoteReceiveData src);
  void dump(const TotoData &data);
};

DECLARE_REMOTE_PROTOCOL(Toto)

}  // namespace esphome::remote_base
