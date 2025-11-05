#pragma once

#include "remote_base.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

static constexpr uint8_t IGNORE_INDEX = 0xFF;

struct DysonData {
  uint16_t code;  // the button, e.g. power, swing, fan++, ...
  uint8_t index;  // the rolling index counter
  bool operator==(const DysonData &rhs) const {
    if (IGNORE_INDEX == index || IGNORE_INDEX == rhs.index) {
      return code == rhs.code;
    }
    return code == rhs.code && index == rhs.index;
  }
};

class DysonProtocol : public RemoteProtocol<DysonData> {
 public:
  void encode(RemoteTransmitData *dst, const DysonData &data) override;
  optional<DysonData> decode(RemoteReceiveData src) override;
  void dump(const DysonData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Dyson)

template<typename... Ts> class DysonAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, code)
  TEMPLATABLE_VALUE(uint8_t, index)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    DysonData data{};
    data.code = this->code_.value(x...);
    data.index = this->index_.value(x...);
    DysonProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
