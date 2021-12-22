#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct PanasonicData {
  uint16_t address;
  uint32_t command;

  bool operator==(const PanasonicData &rhs) const { return address == rhs.address && command == rhs.command; }
};

class PanasonicProtocol : public RemoteProtocol<PanasonicData> {
 public:
  void encode(RemoteTransmitData *dst, const PanasonicData &data) override;
  optional<PanasonicData> decode(RemoteReceiveData src) override;
  void dump(const PanasonicData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Panasonic)

template<typename... Ts> class PanasonicAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint32_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    PanasonicData data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    PanasonicProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
