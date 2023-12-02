#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct RC5Data {
  uint8_t address;
  uint8_t command;

  bool operator==(const RC5Data &rhs) const { return address == rhs.address && command == rhs.command; }
};

class RC5Protocol : public RemoteProtocol<RC5Data> {
 public:
  void encode(RemoteTransmitData *dst, const RC5Data &data) override;
  optional<RC5Data> decode(RemoteReceiveData src) override;
  void dump(const RC5Data &data) override;
};

DECLARE_REMOTE_PROTOCOL(RC5)

template<typename... Ts> class RC5Action : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, address)
  TEMPLATABLE_VALUE(uint8_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    RC5Data data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    RC5Protocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
