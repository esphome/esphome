#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct NECData {
  uint16_t address;
  uint16_t command;

  bool operator==(const NECData &rhs) const { return address == rhs.address && command == rhs.command; }
};

class NECProtocol : public RemoteProtocol<NECData> {
 public:
  void encode(RemoteTransmitData *dst, const NECData &data) override;
  optional<NECData> decode(RemoteReceiveData src) override;
  void dump(const NECData &data) override;
};

DECLARE_REMOTE_PROTOCOL(NEC)

template<typename... Ts> class NECAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint16_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    NECData data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    NECProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
