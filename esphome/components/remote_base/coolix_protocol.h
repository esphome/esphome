#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

using CoolixData = uint32_t;

class CoolixProtocol : public RemoteProtocol<CoolixData> {
 public:
  void encode(RemoteTransmitData *dst, const CoolixData &data) override;
  optional<CoolixData> decode(RemoteReceiveData data) override;
  void dump(const CoolixData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Coolix)

template<typename... Ts> class CoolixAction : public RemoteTransmitterActionBase<Ts...> {
  TEMPLATABLE_VALUE(CoolixData, data)
  void encode(RemoteTransmitData *dst, Ts... x) override {
    CoolixData data = this->data_.value(x...);
    CoolixProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
