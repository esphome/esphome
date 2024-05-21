#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct MirageData {
  std::vector<uint8_t> data;

  bool operator==(const MirageData &rhs) const { return data == rhs.data; }
};

class MirageProtocol : public RemoteProtocol<MirageData> {
 public:
  void encode(RemoteTransmitData *dst, const MirageData &data) override;
  optional<MirageData> decode(RemoteReceiveData src) override;
  void dump(const MirageData &data) override;

 protected:
  void encode_byte_(RemoteTransmitData *dst, uint8_t item);
};

DECLARE_REMOTE_PROTOCOL(Mirage)

template<typename... Ts> class MirageAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, code)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    MirageData data{};
    data.data = this->code_.value(x...);
    MirageProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
