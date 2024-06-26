#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct RC6Data {
  uint8_t mode : 3;
  uint8_t toggle : 1;
  uint8_t address;
  uint8_t command;

  bool operator==(const RC6Data &rhs) const { return address == rhs.address && command == rhs.command; }
};

class RC6Protocol : public RemoteProtocol<RC6Data> {
 public:
  void encode(RemoteTransmitData *dst, const RC6Data &data) override;
  optional<RC6Data> decode(RemoteReceiveData src) override;
  void dump(const RC6Data &data) override;
};

DECLARE_REMOTE_PROTOCOL(RC6)

template<typename... Ts> class RC6Action : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, address)
  TEMPLATABLE_VALUE(uint8_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) {
    RC6Data data{};
    data.mode = 0;
    data.toggle = this->toggle_;
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    RC6Protocol().encode(dst, data);
    this->toggle_ = !this->toggle_;
  }

 protected:
  uint8_t toggle_{0};
};

}  // namespace remote_base
}  // namespace esphome
