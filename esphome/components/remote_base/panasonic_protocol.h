#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct PanasonicData {
  uint16_t address;
  uint8_t address2;
  uint32_t command;
  uint16_t nbits;
  uint16_t carrier_frequency;

  bool operator==(const PanasonicData &rhs) const {
    return address == rhs.address && address2 == rhs.address2 && command == rhs.command && nbits == rhs.nbits &&
           carrier_frequency == rhs.carrier_frequency;
  }
};

class PanasonicProtocol : public RemoteProtocol<PanasonicData> {
 public:
  void encode(RemoteTransmitData *dst, const PanasonicData &data);
  optional<PanasonicData> decode(RemoteReceiveData src);
  void dump(const PanasonicData &data);
};

DECLARE_REMOTE_PROTOCOL(Panasonic)

template<typename... Ts> class PanasonicAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(uint8_t, address2)
  TEMPLATABLE_VALUE(uint32_t, command)
  TEMPLATABLE_VALUE(uint16_t, nbits)
  TEMPLATABLE_VALUE(uint16_t, carrier_frequency)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    PanasonicData data{};
    data.address = this->address_.value(x...);
    data.address2 = this->address2_.value(x...);
    data.command = this->command_.value(x...);
    data.nbits = this->nbits_.value(x...);
    data.carrier_frequency = this->carrier_frequency_.value(x...);
    PanasonicProtocol().encode(dst, data);
  }
};

}  // namespace esphome::remote_base
