#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct DraytonData {
  uint16_t address;
  uint8_t data;
  uint8_t channel;
  uint8_t command;

  // channel and command are the legacy split of the same packet bits as data plus the checksum,
  // so only address and data are compared; received packets have a verified checksum.
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
