#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct PanasonicData {
  uint16_t address;
  uint32_t command;

  bool operator==(const PanasonicData &rhs) const { return address == rhs.address && command == rhs.command; }
};

class PanasonicProtocol : public RemoteProtocol<PanasonicData> {
 public:
  void encode(RemoteTransmitData *dst, const PanasonicData &data);
  optional<PanasonicData> decode(RemoteReceiveData src);
  void dump(const PanasonicData &data);
};

DECLARE_REMOTE_PROTOCOL(Panasonic)

}  // namespace esphome::remote_base
