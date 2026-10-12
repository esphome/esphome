#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct PanasonicData {
  // address and command stay first so positional initializers written for the 48-bit struct keep working.
  uint16_t address;
  uint32_t command;
  uint8_t nbits{48};  // 48 or 56
  uint8_t address_2{0};
  // Transmit only: a receiver cannot measure the carrier, so it is not compared.
  uint32_t carrier_frequency{35000};

  bool operator==(const PanasonicData &rhs) const {
    return address == rhs.address && address_2 == rhs.address_2 && command == rhs.command && nbits == rhs.nbits;
  }
};

class PanasonicProtocol : public RemoteProtocol<PanasonicData> {
 public:
  void encode(RemoteTransmitData *dst, const PanasonicData &data);
  optional<PanasonicData> decode(RemoteReceiveData src);
  void dump(const PanasonicData &data);
};

DECLARE_REMOTE_PROTOCOL(Panasonic)

}  // namespace esphome::remote_base
