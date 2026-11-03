#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct NexusData {
  float temperature;  // °C, sent as 12 bit signed tenths
  uint8_t address;
  bool battery_level;
  bool force_update;
  uint8_t channel;  // sent 0-based, shown 1-based
  uint8_t humidity;

  bool operator==(const NexusData &rhs) const {
    // The binary sensor matches the transmitter, not the reading.
    return channel == rhs.channel && address == rhs.address;
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
