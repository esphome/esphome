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

  bool operator==(const DraytonData &rhs) const {
    return address == rhs.address && channel == rhs.channel && command == rhs.command && data == rhs.data;
  }
};

class DraytonProtocol : public RemoteProtocol<DraytonData> {
 public:
  void encode(RemoteTransmitData *dst, const DraytonData &data);
  optional<DraytonData> decode(RemoteReceiveData src);
  void dump(const DraytonData &data);

 protected:
  uint8_t calc_cs_(uint32_t) const;
  void encode_(RemoteTransmitData *dst, const DraytonData &data);
  optional<DraytonData> decode_(RemoteReceiveData src);
  void dump_(const DraytonData &data);
};

DECLARE_REMOTE_PROTOCOL(Drayton)

}  // namespace esphome::remote_base
