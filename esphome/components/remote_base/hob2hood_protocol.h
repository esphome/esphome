#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

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

  std::string to_string() const {
    switch (this->command) {
      case HOB2HOOD_CMD_LIGHT_OFF:
        return "Light off";
      case HOB2HOOD_CMD_LIGHT_ON:
        return "Light on";
      case HOB2HOOD_CMD_FAN_OFF:
        return "Fan off";
      case HOB2HOOD_CMD_FAN_LOW:
        return "Fan low";
      case HOB2HOOD_CMD_FAN_MEDIUM:
        return "Fan medium";
      case HOB2HOOD_CMD_FAN_HIGH:
        return "Fan high";
      case HOB2HOOD_CMD_FAN_MAX:
        return "Fan max";
      default:
        return "Unknown";
    }
  }
};

class Hob2HoodProtocol : public RemoteProtocol<Hob2HoodData> {
 public:
  void encode(RemoteTransmitData *dst, const Hob2HoodData &data) override;
  optional<Hob2HoodData> decode(RemoteReceiveData src) override;
  void dump(const Hob2HoodData &data) override;

 protected:
  void encode_data_(RemoteTransmitData *dst, const std::vector<int8_t> &data) const;
  bool expect_data_(RemoteReceiveData &src, int8_t data);
  bool expect_data_(RemoteReceiveData &src, const std::vector<int8_t> &data);
};

using Hob2HoodTrigger = RemoteReceiverTrigger<Hob2HoodProtocol>;

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
  TEMPLATABLE_VALUE(Hob2HoodCommand, command)
  void encode(RemoteTransmitData *dst, Ts... x) override {
    Hob2HoodData data;
    data.command = this->command_.value(x...);
    Hob2HoodProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
