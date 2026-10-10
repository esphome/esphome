#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct OnkyoRIData {
  uint16_t data;

  bool operator==(const OnkyoRIData &rhs) const { return data == rhs.data; }
};

class OnkyoRIProtocol : public RemoteProtocol<OnkyoRIData> {
 public:
  void encode(RemoteTransmitData *dst, const OnkyoRIData &data);
  optional<OnkyoRIData> decode(RemoteReceiveData src);
  void dump(const OnkyoRIData &data);
};

DECLARE_REMOTE_PROTOCOL(OnkyoRI)

template<typename... Ts> class OnkyoRIAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, data)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    OnkyoRIData data{};
    data.data = this->data_.value(x...);
    OnkyoRIProtocol().encode(dst, data);
  }
};

}  // namespace esphome::remote_base
