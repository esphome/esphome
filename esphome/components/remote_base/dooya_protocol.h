#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct DooyaData {
  uint32_t id;
  uint8_t channel;
  uint8_t button;
  uint8_t check;

  bool operator==(const DooyaData &rhs) const {
    return id == rhs.id && channel == rhs.channel && button == rhs.button && check == rhs.check;
  }
};

class DooyaProtocol : public RemoteProtocol<DooyaData> {
 public:
  void encode(RemoteTransmitData *dst, const DooyaData &data);
  optional<DooyaData> decode(RemoteReceiveData src);
  void dump(const DooyaData &data);
};

DECLARE_REMOTE_PROTOCOL(Dooya)

}  // namespace esphome::remote_base
