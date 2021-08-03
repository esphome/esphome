#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct Samsung36Data {
  uint16_t address;
  uint32_t command;

  bool operator==(const Samsung36Data &rhs) const { return address == rhs.address && command == rhs.command; }
};

class Samsung36Protocol : public RemoteProtocol<Samsung36Data> {
 public:
  void encode(RemoteTransmitData *dst, const Samsung36Data &data) override;
  optional<Samsung36Data> decode(RemoteReceiveData src) override;
  void dump(const Samsung36Data &data) override;
};

DECLARE_REMOTE_PROTOCOL(Samsung36)

template<typename... Ts> class Samsung36Action : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint32_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    Samsung36Data data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    Samsung36Protocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
