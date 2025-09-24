#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

struct DraytonData {
  uint16_t address;
  uint8_t channel;
  uint8_t command;

  bool operator==(const DraytonData &rhs) const {
    return address == rhs.address && channel == rhs.channel && command == rhs.command;
  }
};

class DraytonProtocol : public RemoteProtocol<DraytonData> {
 public:
  void encode(RemoteTransmitData *dst, const DraytonData &data) override;
  optional<DraytonData> decode(RemoteReceiveData src) override;
  void dump(const DraytonData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Drayton)

template<typename... Ts> class DraytonAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint8_t, channel)
  TEMPLATABLE_VALUE(uint8_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    DraytonData data{};
    data.address = this->address_.value(x...);
    data.channel = this->channel_.value(x...);
    data.command = this->command_.value(x...);
    DraytonProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
