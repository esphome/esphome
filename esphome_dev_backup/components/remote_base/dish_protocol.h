#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct DishData {
  uint8_t address;
  uint8_t command;

  bool operator==(const DishData &rhs) const { return address == rhs.address && command == rhs.command; }
};

class DishProtocol : public RemoteProtocol<DishData> {
 public:
  void encode(RemoteTransmitData *dst, const DishData &data) override;
  optional<DishData> decode(RemoteReceiveData src) override;
  void dump(const DishData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Dish)

template<typename... Ts> class DishAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, address)
  TEMPLATABLE_VALUE(uint8_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    DishData data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    DishProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
