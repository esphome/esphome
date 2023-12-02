#pragma once

#include "remote_base.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

struct NexaData {
  uint32_t device;
  uint8_t group;
  uint8_t state;
  uint8_t channel;
  uint8_t level;
  bool operator==(const NexaData &rhs) const {
    return device == rhs.device && group == rhs.group && state == rhs.state && channel == rhs.channel &&
           level == rhs.level;
  }
};

class NexaProtocol : public RemoteProtocol<NexaData> {
 public:
  void one(RemoteTransmitData *dst) const;
  void zero(RemoteTransmitData *dst) const;
  void sync(RemoteTransmitData *dst) const;

  void encode(RemoteTransmitData *dst, const NexaData &data) override;
  optional<NexaData> decode(RemoteReceiveData src) override;
  void dump(const NexaData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Nexa)

template<typename... Ts> class NexaAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, device)
  TEMPLATABLE_VALUE(uint8_t, group)
  TEMPLATABLE_VALUE(uint8_t, state)
  TEMPLATABLE_VALUE(uint8_t, channel)
  TEMPLATABLE_VALUE(uint8_t, level)
  void encode(RemoteTransmitData *dst, Ts... x) override {
    NexaData data{};
    data.device = this->device_.value(x...);
    data.group = this->group_.value(x...);
    data.state = this->state_.value(x...);
    data.channel = this->channel_.value(x...);
    data.level = this->level_.value(x...);
    NexaProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
