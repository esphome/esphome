#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct ToshibaAcData {
  uint64_t rc_code_1;
  uint64_t rc_code_2;

  bool operator==(const ToshibaAcData &rhs) const { return rc_code_1 == rhs.rc_code_1 && rc_code_2 == rhs.rc_code_2; }
};

class ToshibaAcProtocol : public RemoteProtocol<ToshibaAcData> {
 public:
  void encode(RemoteTransmitData *dst, const ToshibaAcData &data) override;
  optional<ToshibaAcData> decode(RemoteReceiveData src) override;
  void dump(const ToshibaAcData &data) override;
};

DECLARE_REMOTE_PROTOCOL(ToshibaAc)

template<typename... Ts> class ToshibaAcAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint64_t, rc_code_1)
  TEMPLATABLE_VALUE(uint64_t, rc_code_2)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    ToshibaAcData data{};
    data.rc_code_1 = this->rc_code_1_.value(x...);
    data.rc_code_2 = this->rc_code_2_.value(x...);
    ToshibaAcProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
