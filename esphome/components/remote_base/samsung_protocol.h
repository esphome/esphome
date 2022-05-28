#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct SamsungData {
  uint64_t data;
  uint16_t address;
  uint16_t command;

  bool operator==(const SamsungData &rhs) const {
    return data == rhs.data && address == rhs.address && command == rhs.command;
  }
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
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint16_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    SamsungData data{};
    data.data = this->data_.value(x...);
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    SamsungProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
