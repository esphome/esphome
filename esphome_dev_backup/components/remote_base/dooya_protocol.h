#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

struct DooyaData {
  uint32_t id;
  uint8_t channel;
  uint8_t button;
  uint8_t check;

  bool operator==(const DooyaData &rhs) const {
    return id == rhs.id && channel == rhs.channel && button == rhs.button && check == rhs.check;
  }
};

class DooyaProtocol : public RemoteProtocol<DooyaData> {
 public:
  void encode(RemoteTransmitData *dst, const DooyaData &data) override;
  optional<DooyaData> decode(RemoteReceiveData src) override;
  void dump(const DooyaData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Dooya)

template<typename... Ts> class DooyaAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, id)
  TEMPLATABLE_VALUE(uint8_t, channel)
  TEMPLATABLE_VALUE(uint8_t, button)
  TEMPLATABLE_VALUE(uint8_t, check)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    DooyaData data{};
    data.id = this->id_.value(x...);
    data.channel = this->channel_.value(x...);
    data.button = this->button_.value(x...);
    data.check = this->check_.value(x...);
    DooyaProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
