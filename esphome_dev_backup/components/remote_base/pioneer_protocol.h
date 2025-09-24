#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct PioneerData {
  uint16_t rc_code_1;
  uint16_t rc_code_2;

  bool operator==(const PioneerData &rhs) const { return rc_code_1 == rhs.rc_code_1 && rc_code_2 == rhs.rc_code_2; }
};

class PioneerProtocol : public RemoteProtocol<PioneerData> {
 public:
  void encode(RemoteTransmitData *dst, const PioneerData &data) override;
  optional<PioneerData> decode(RemoteReceiveData src) override;
  void dump(const PioneerData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Pioneer)

template<typename... Ts> class PioneerAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, rc_code_1)
  TEMPLATABLE_VALUE(uint16_t, rc_code_2)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    PioneerData data{};
    data.rc_code_1 = this->rc_code_1_.value(x...);
    data.rc_code_2 = this->rc_code_2_.value(x...);
    PioneerProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
