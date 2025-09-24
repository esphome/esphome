#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

struct LGData {
  uint32_t data;
  uint8_t nbits;

  bool operator==(const LGData &rhs) const { return data == rhs.data && nbits == rhs.nbits; }
};

class LGProtocol : public RemoteProtocol<LGData> {
 public:
  void encode(RemoteTransmitData *dst, const LGData &data) override;
  optional<LGData> decode(RemoteReceiveData src) override;
  void dump(const LGData &data) override;
};

DECLARE_REMOTE_PROTOCOL(LG)

template<typename... Ts> class LGAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, data)
  TEMPLATABLE_VALUE(uint8_t, nbits)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    LGData data{};
    data.data = this->data_.value(x...);
    data.nbits = this->nbits_.value(x...);
    LGProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
