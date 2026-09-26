#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

enum Hob2HoodCommand : uint8_t {
  HOB2HOOD_COMMAND_LIGHT_OFF = 0xd5,
  HOB2HOOD_COMMAND_LIGHT_ON = 0xd2,
  HOB2HOOD_COMMAND_FAN_OFF = 0xd8,
  HOB2HOOD_COMMAND_FAN_LOW = 0x6c,
  HOB2HOOD_COMMAND_FAN_MEDIUM = 0x6f,
  HOB2HOOD_COMMAND_FAN_HIGH = 0xe1,
  HOB2HOOD_COMMAND_FAN_MAX = 0x72,
};

struct Hob2HoodData {
  Hob2HoodCommand command;
  bool operator==(const Hob2HoodData &rhs) const { return this->command == rhs.command; }
};

class Hob2HoodProtocol : public RemoteProtocol<Hob2HoodData> {
 public:
  void encode(RemoteTransmitData *dst, const Hob2HoodData &data);
  optional<Hob2HoodData> decode(RemoteReceiveData src);
  void dump(const Hob2HoodData &data);
};

DECLARE_REMOTE_PROTOCOL(Hob2Hood)

template<typename... Ts> class Hob2HoodAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(Hob2HoodCommand, command)
  void encode(RemoteTransmitData *dst, Ts... x) override {
    Hob2HoodProtocol().encode(dst, Hob2HoodData{this->command_.value(x...)});
  }
};

}  // namespace esphome::remote_base
