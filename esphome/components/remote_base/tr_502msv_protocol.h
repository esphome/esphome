#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

enum Devices : uint8_t {
  DEVICE_1 = 0,
  DEVICE_2 = 1,
  DEVICE_3 = 2,
  DEVICE_4 = 3,
  DEVICE_ALL = 7,
};

enum Commands : uint8_t {
  COMMAND_OFF = 0,
  COMMAND_ON = 1,
  COMMAND_INCREASE_BRIGHTNESS = 2,
  COMMAND_DECREASE_BRIGHTNESS = 3,
};

struct TR502MSVData {
  union {
    struct {
      uint16_t group : 12;
      uint8_t device : 3;
      uint8_t command : 2;
      uint8_t _reserved : 1;
      uint8_t checksum : 2;
    } __attribute__((packed));
    uint32_t raw;
  };

  bool operator==(const TR502MSVData &rhs) const { return this->raw == rhs.raw; }

  std::string device_string() const;
  std::string command_string() const;

  uint8_t calc_cs() const { return (device & 3) ^ ((device >> 2) & 1) ^ ((command >> 1) & 1) ^ ((command << 1) & 2); }
};

class TR502MSVProtocol : public RemoteProtocol<TR502MSVData> {
 public:
  void encode(RemoteTransmitData *dst, const TR502MSVData &data) override;
  optional<TR502MSVData> decode(RemoteReceiveData src) override;
  void dump(const TR502MSVData &data) override;
};

DECLARE_REMOTE_PROTOCOL(TR502MSV)

template<typename... Ts> class TR502MSVAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, group)
  TEMPLATABLE_VALUE(uint8_t, device)
  TEMPLATABLE_VALUE(uint8_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    TR502MSVData data{};
    data.group = this->group_.value(x...);
    data.device = this->device_.value(x...);
    data.command = this->command_.value(x...);
    data.checksum = data.calc_cs();
    TR502MSVProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
