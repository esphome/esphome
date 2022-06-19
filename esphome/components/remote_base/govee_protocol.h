#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct GoveeData {
  uint16_t address;
  uint8_t command;
  uint16_t cmdopt;
  uint8_t crc;
  uint32_t repeat;
};

class GoveeProtocol : public RemoteProtocol<GoveeData> {
 public:
  void encode(RemoteTransmitData *dst, const GoveeData &data) override;
  optional<GoveeData> decode(RemoteReceiveData src) override;
  void dump(const GoveeData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Govee)

template<typename... Ts> class GoveeAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint8_t, command)
  TEMPLATABLE_VALUE(uint16_t, cmdopt)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    GoveeData data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    data.cmdopt = this->cmdopt_.value(x...);
    data.repeat = this->repeat_.value(x...);
    GoveeProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
