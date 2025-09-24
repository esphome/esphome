#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct SamsungData {
  uint64_t data;
  uint8_t nbits;

  bool operator==(const SamsungData &rhs) const { return data == rhs.data && nbits == rhs.nbits; }
};

class SamsungProtocol : public RemoteProtocol<SamsungData> {
 public:
  void encode(RemoteTransmitData *dst, const SamsungData &data) override;
  optional<SamsungData> decode(RemoteReceiveData src) override;
  void dump(const SamsungData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Samsung)

template<typename... Ts> class SamsungAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint64_t, data)
  TEMPLATABLE_VALUE(uint8_t, nbits)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    SamsungData data{};
    data.data = this->data_.value(x...);
    data.nbits = this->nbits_.value(x...);
    SamsungProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
