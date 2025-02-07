#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct TotoData {
  uint8_t rc_code_1 : 4;
  uint8_t rc_code_2 : 4;
  uint8_t command;

  bool operator==(const TotoData &rhs) const {
    return (rc_code_1 == rhs.rc_code_1) && (rc_code_2 == rhs.rc_code_2) && (command == rhs.command);
  }
};

class TotoProtocol : public RemoteProtocol<TotoData> {
 public:
  void encode(RemoteTransmitData *dst, const TotoData &data) override;
  optional<TotoData> decode(RemoteReceiveData src) override;
  void dump(const TotoData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Toto)

template<typename... Ts> class TotoAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, rc_code_1)
  TEMPLATABLE_VALUE(uint8_t, rc_code_2)
  TEMPLATABLE_VALUE(uint8_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    TotoData data{};
    data.rc_code_1 = this->rc_code_1_.value(x...);
    data.rc_code_2 = this->rc_code_2_.value(x...);
    data.command = this->command_.value(x...);
    this->set_send_times(this->send_times_.value_or(x..., 3));
    this->set_send_wait(this->send_wait_.value_or(x..., 32000));
    TotoProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
