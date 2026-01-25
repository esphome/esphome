#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct FSLScoreboardData {
  uint8_t field;
  uint16_t value;

  bool operator==(const FSLScoreboardData &rhs) const { return field == rhs.field && value == rhs.value; }
};

class FSLScoreboardProtocol : public RemoteProtocol<FSLScoreboardData> {
 public:
  void encode(RemoteTransmitData *dst, const FSLScoreboardData &data) override;
  optional<FSLScoreboardData> decode(RemoteReceiveData src) override;
  void dump(const FSLScoreboardData &data) override;
};

DECLARE_REMOTE_PROTOCOL(FSLScoreboard)

template<typename... Ts> class FSLScoreboardAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, field)
  TEMPLATABLE_VALUE(uint16_t, value)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    FSLScoreboardData data{};
    data.field = this->field_.value(x...);
    data.value = this->value_.value(x...);
    FSLScoreboardProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
