#pragma once

#include "esphome/core/component.h"
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
  void encode(RemoteTransmitData *dst, const NexusData &data) override;
  optional<NexusData> decode(RemoteReceiveData src) override;
  void dump(const NexusData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Nexus)

template<typename... Ts> class NexusAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, channel)
  TEMPLATABLE_VALUE(uint8_t, address)
  TEMPLATABLE_VALUE(float, temperature)
  TEMPLATABLE_VALUE(uint8_t, humidity)
  TEMPLATABLE_VALUE(bool, battery_level)
  TEMPLATABLE_VALUE(bool, force_update)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    NexusData data{};
    data.channel = this->channel_.value(x...);
    data.address = this->address_.value(x...);
    data.temperature = this->temperature_.value(x...);
    data.humidity = this->humidity_.value(x...);
    data.battery_level = this->battery_level_.value(x...);
    data.force_update = this->force_update_.value(x...);
    NexusProtocol().encode(dst, data);
  }
};

}  // namespace esphome::remote_base
