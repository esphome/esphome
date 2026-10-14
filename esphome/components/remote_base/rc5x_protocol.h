#pragma once

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

  bool operator==(const RC5XData &rhs) const {
    return address == rhs.address && command == rhs.command && extension == rhs.extension;
  }
};

class RC5XProtocol : public RemoteProtocol<RC5XData> {
 public:
  void encode(RemoteTransmitData *dst, const RC5XData &data);
  optional<RC5XData> decode(RemoteReceiveData src);
  void dump(const RC5XData &data);
};

DECLARE_REMOTE_PROTOCOL(RC5X)

}  // namespace esphome::remote_base
