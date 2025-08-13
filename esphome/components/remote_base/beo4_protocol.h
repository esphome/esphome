#pragma once

#include "remote_base.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

struct Beo4Data {
  uint8_t source;   // beoSource, e.g. video, audio, light...
  uint8_t command;  // beoCommend, e.g. volume+, mute,...
  uint8_t repeats;  // beoRepeat for repeat commands, e.g. up, down...

  bool operator==(const Beo4Data &rhs) const { return source == rhs.source && command == rhs.command; }
};

class Beo4Protocol : public RemoteProtocol<Beo4Data> {
 public:
  void encode(RemoteTransmitData *dst, const Beo4Data &data) override;
  optional<Beo4Data> decode(RemoteReceiveData src) override;
  void dump(const Beo4Data &data) override;
};

DECLARE_REMOTE_PROTOCOL(Beo4)

template<typename... Ts> class Beo4Action : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, source)
  TEMPLATABLE_VALUE(uint8_t, command)
  TEMPLATABLE_VALUE(uint8_t, repeats)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    Beo4Data data{};
    data.source = this->source_.value(x...);
    data.command = this->command_.value(x...);
    data.repeats = this->repeats_.value(x...);
    Beo4Protocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
