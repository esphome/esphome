#pragma once

#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct NexusData {
  uint8_t channel;
  uint8_t address;
  float temperature;
  uint8_t humidity;
  bool battery_level;
  bool force_update;

  bool operator==(const NexusData &rhs) const {
    return channel == rhs.channel && address == rhs.address && temperature == rhs.temperature &&
           humidity == rhs.humidity && battery_level == rhs.battery_level && force_update == rhs.force_update;
  }
};

class NexusProtocol : public RemoteProtocol<NexusData> {
 public:
  void encode(RemoteTransmitData *dst, const NexusData &data);
  optional<NexusData> decode(RemoteReceiveData src);
  void dump(const NexusData &data);
};

DECLARE_REMOTE_PROTOCOL(Nexus)

}  // namespace esphome::remote_base
