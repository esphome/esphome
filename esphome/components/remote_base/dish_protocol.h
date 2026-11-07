#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct DishData {
  uint8_t address;
  uint8_t command;

  bool operator==(const DishData &rhs) const { return address == rhs.address && command == rhs.command; }
};

class DishProtocol : public RemoteProtocol<DishData> {
 public:
  void encode(RemoteTransmitData *dst, const DishData &data);
  optional<DishData> decode(RemoteReceiveData src);
  void dump(const DishData &data);
};

DECLARE_REMOTE_PROTOCOL(Dish)

}  // namespace esphome::remote_base
