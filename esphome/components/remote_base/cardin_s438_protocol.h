#pragma once

#include "esphome/components/remote_base/remote_base.h"

namespace esphome::remote_base {

struct CardinS438Data {
  uint16_t code{0};
  uint8_t channel{0};
  uint8_t seed{0};
  uint8_t repeats{3};

  bool operator==(const CardinS438Data &rhs) const { return code == rhs.code && channel == rhs.channel; }
};

class CardinS438Protocol : public RemoteProtocol<CardinS438Data> {
 public:
  void encode(RemoteTransmitData *dst, const CardinS438Data &data);
  optional<CardinS438Data> decode(RemoteReceiveData src);
  void dump(const CardinS438Data &data);
};

DECLARE_REMOTE_PROTOCOL(CardinS438)

}  // namespace esphome::remote_base
