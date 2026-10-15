#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct DraytonData {
  uint16_t address;
  uint8_t channel;
  uint8_t command;
  uint8_t data;  // last, so positional initializers written for the legacy layout keep their meaning

  // channel/command: legacy split of the same bits, filled on decode, ignored by operator==
  bool operator==(const DraytonData &rhs) const { return address == rhs.address && data == rhs.data; }
};

class DraytonProtocol : public RemoteProtocol<DraytonData> {
 public:
  void encode(RemoteTransmitData *dst, const DraytonData &data);
  optional<DraytonData> decode(RemoteReceiveData src);
  void dump(const DraytonData &data);
};

DECLARE_REMOTE_PROTOCOL(Drayton)

}  // namespace esphome::remote_base
