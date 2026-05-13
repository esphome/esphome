#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

/*
 * Marantz RC5 extended protocol (RC5x)
 * See https://arduino-irremote.github.io/Arduino-IRremote/ir__RC5__RC6_8hpp_source.html
 */

namespace esphome::remote_base {

struct RC5XData {
  uint8_t address;
  uint8_t command;
  uint8_t extension;
};

class RC5XProtocol : public RemoteProtocol<RC5XData> {
 public:
  void encode(RemoteTransmitData *dst, const RC5XData &data) override;
  optional<RC5XData> decode(RemoteReceiveData src) override;
  void dump(const RC5XData &data) override;
};

DECLARE_REMOTE_PROTOCOL(RC5X)

template<typename... Ts> class RC5XAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, address)
  TEMPLATABLE_VALUE(uint8_t, command)
  TEMPLATABLE_VALUE(uint8_t, extension)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    RC5XData data{};
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    data.extension = this->extension_.value(x...);
    RC5XProtocol().encode(dst, data);
  }
};

}  // namespace esphome::remote_base
