#pragma once

#include "remote_base.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

struct OnkyoRIData {
  uint32_t data;

  bool operator==(const OnkyoRIData &rhs) const { return data == rhs.data; }
};

class OnkyoRIProtocol : public RemoteProtocol<OnkyoRIData> {
 public:
  void encode(RemoteTransmitData *dst, const OnkyoRIData &data) override;
  optional<OnkyoRIData> decode(RemoteReceiveData src) override;
  void dump(const OnkyoRIData &data) override;
};

DECLARE_REMOTE_PROTOCOL(OnkyoRI)

template<typename... Ts> class OnkyoRIAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, data)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    OnkyoRIData data{};
    data.data = this->data_.value(x...);
    OnkyoRIProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
