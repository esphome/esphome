#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct RoombaData {
  uint8_t data;

  bool operator==(const RoombaData &rhs) const { return data == rhs.data; }
};

class RoombaProtocol : public RemoteProtocol<RoombaData> {
 public:
  void encode(RemoteTransmitData *dst, const RoombaData &data);
  optional<RoombaData> decode(RemoteReceiveData src);
  void dump(const RoombaData &data);
};

DECLARE_REMOTE_PROTOCOL(Roomba)

}  // namespace esphome::remote_base
