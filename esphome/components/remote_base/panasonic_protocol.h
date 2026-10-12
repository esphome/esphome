#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct PanasonicData {
  uint16_t nbits;
  uint16_t address;
  uint8_t address_2;
  uint32_t command;
  uint32_t carrier_frequency;

  bool operator==(const PanasonicData &rhs) const {
    return address == rhs.address && address_2 == rhs.address_2 && command == rhs.command && nbits == rhs.nbits &&
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

}  // namespace esphome::remote_base
