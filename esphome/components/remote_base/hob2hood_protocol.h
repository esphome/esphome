#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

enum Hob2HoodCommand : uint8_t {
  HOB2HOOD_CMD_LIGHT_OFF = 0,
  HOB2HOOD_CMD_LIGHT_ON,
  HOB2HOOD_CMD_FAN_OFF,
  HOB2HOOD_CMD_FAN_LOW,
  HOB2HOOD_CMD_FAN_MEDIUM,
  HOB2HOOD_CMD_FAN_HIGH,
  HOB2HOOD_CMD_FAN_MAX,
  HOB2HOOD_CMD_UNKNOWN,
};

struct Hob2HoodData {
  Hob2HoodData() {}
  Hob2HoodData(Hob2HoodCommand command) { this->command = command; }

  Hob2HoodCommand command{HOB2HOOD_CMD_UNKNOWN};

  bool operator==(const Hob2HoodData &rhs) const { return this->command == rhs.command; }
};

class Hob2HoodProtocol : public RemoteProtocol<Hob2HoodData> {
 public:
  void encode(RemoteTransmitData *dst, const Hob2HoodData &data) override;
  optional<Hob2HoodData> decode(RemoteReceiveData src) override;
  void dump(const Hob2HoodData &data) override;

 protected:
  void encode_data_(RemoteTransmitData *dst, const int8_t *data, size_t length) const;
  bool expect_data_(RemoteReceiveData &src, int8_t data);
  bool expect_data_(RemoteReceiveData &src, const int8_t *data, size_t length);
};

using Hob2HoodTrigger = RemoteReceiverTrigger<Hob2HoodProtocol>;
using Hob2HoodDumper = RemoteReceiverDumper<Hob2HoodProtocol>;

class Hob2HoodBinarySensor : public RemoteReceiverBinarySensorBase {
 public:
  bool matches(RemoteReceiveData src) override {
    auto data = Hob2HoodProtocol().decode(src);
    return data.has_value() && data.value() == this->data_;
  }
  void set_command(const Hob2HoodCommand command) { this->data_.command = command; }

 protected:
  Hob2HoodData data_;
};

template<typename... Ts> class Hob2HoodAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, command)
  void encode(RemoteTransmitData *dst, Ts... x) override {
    Hob2HoodData data;
    data.command = (Hob2HoodCommand) this->command_.value(x...);
    Hob2HoodProtocol().encode(dst, data);
  }
};

}  // namespace esphome::remote_base
