#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct AOKData {
  uint32_t remote_id;  // 24 bits
  uint16_t channels;
  uint8_t command;

  bool operator==(const AOKData &rhs) const {
    return this->remote_id == rhs.remote_id && this->channels == rhs.channels && this->command == rhs.command;
  }
};

class AOKProtocol : public RemoteProtocol<AOKData> {
 public:
  void encode(RemoteTransmitData *dst, const AOKData &data);
  optional<AOKData> decode(RemoteReceiveData src);
  void dump(const AOKData &data);
};

DECLARE_REMOTE_PROTOCOL(AOK)

}  // namespace esphome::remote_base
