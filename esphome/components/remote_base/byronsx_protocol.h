#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct ByronSXData {
  uint8_t address;
  uint8_t command;

  bool operator==(const ByronSXData &rhs) const {
    // Treat 0x10 as a special, wildcard command/chime
    // This allows us to match on just the address if wanted.
    if (address != rhs.address) {
      return false;
    }
    return (rhs.command == 0x10 || command == rhs.command);
  }
};

class ByronSXProtocol : public RemoteProtocol<ByronSXData> {
 public:
  void encode(RemoteTransmitData *dst, const ByronSXData &data) override;
  optional<ByronSXData> decode(RemoteReceiveData src) override;
  void dump(const ByronSXData &data) override;
};

DECLARE_REMOTE_PROTOCOL(ByronSX)

template<typename... Ts> class ByronSXAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, address)
  TEMPLATABLE_VALUE(uint8_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    ByronSXData data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    ByronSXProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
