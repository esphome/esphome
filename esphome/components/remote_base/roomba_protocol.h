#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct RoombaData {
  uint8_t data;

  bool operator==(const RoombaData &rhs) const { return data == rhs.data; }
};

class RoombaProtocol : public RemoteProtocol<RoombaData> {
 public:
  void encode(RemoteTransmitData *dst, const RoombaData &data) override;
  optional<RoombaData> decode(RemoteReceiveData src) override;
  void dump(const RoombaData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Roomba)

template<typename... Ts> class RoombaAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, data)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    RoombaData data{};
    data.data = this->data_.value(x...);
    RoombaProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
