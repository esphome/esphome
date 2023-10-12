#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct KeeloqData {
  uint32_t encrypted;  // 32 bit encrypted field
  uint32_t address;    // 28 bit serial number
  uint8_t command;     // Button Status S2-S1-S0-S3
  bool repeat;         // Repeated command bit
  bool vlow;           // Battery status bit

  bool operator==(const KeeloqData &rhs) const {
    // Treat 0x10 as a special, wildcard button press
    // This allows us to match on just the address if wanted.
    if (address != rhs.address) {
      return false;
    }
    return (rhs.command == 0x10 || command == rhs.command);
  }
};

class KeeloqProtocol : public RemoteProtocol<KeeloqData> {
 public:
  void encode(RemoteTransmitData *dst, const KeeloqData &data) override;
  optional<KeeloqData> decode(RemoteReceiveData src) override;
  void dump(const KeeloqData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Keeloq)

template<typename... Ts> class KeeloqAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(uint32_t, encrypted)
  TEMPLATABLE_VALUE(uint8_t, command)
  TEMPLATABLE_VALUE(bool, vlow)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    KeeloqData data{};
    data.address = this->address_.value(x...);
    data.encrypted = this->encrypted_.value(x...);
    data.command = this->command_.value(x...);
    data.vlow = this->vlow_.value(x...);
    KeeloqProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
