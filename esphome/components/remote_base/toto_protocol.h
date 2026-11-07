#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct TotoData {
  uint16_t code;
  // The fields of the old most-significant-bit-first reading of code, filled on receive so that older
  // on_toto lambdas keep working
  uint8_t rc_code_1 : 4;
  uint8_t rc_code_2 : 4;
  uint8_t command;

  bool operator==(const TotoData &rhs) const { return this->code == rhs.code; }
};

class TotoProtocol : public RemoteProtocol<TotoData> {
 public:
  void encode(RemoteTransmitData *dst, const TotoData &data);
  optional<TotoData> decode(RemoteReceiveData src);
  void dump(const TotoData &data);
};

DECLARE_REMOTE_PROTOCOL(Toto)

}  // namespace esphome::remote_base
