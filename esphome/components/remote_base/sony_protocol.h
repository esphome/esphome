#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

struct SonyData {
  uint32_t data;
  uint8_t nbits;

  bool operator==(const SonyData &rhs) const { return data == rhs.data && nbits == rhs.nbits; }
};

class SonyProtocol : public RemoteProtocol<SonyData> {
 public:
  void encode(RemoteTransmitData *dst, const SonyData &data) override;
  optional<SonyData> decode(RemoteReceiveData src) override;
  void dump(const SonyData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Sony)

template<typename... Ts> class SonyAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, data)
  TEMPLATABLE_VALUE(uint8_t, nbits)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    SonyData data{};
    data.data = this->data_.value(x...);
    data.nbits = this->nbits_.value(x...);
    SonyProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
