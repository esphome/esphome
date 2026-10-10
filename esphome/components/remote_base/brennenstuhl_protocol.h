#pragma once

#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct BrennenstuhlData {
  uint32_t code;
  bool operator==(const BrennenstuhlData &rhs) const { return code == rhs.code; }
};

class BrennenstuhlProtocol : public RemoteProtocol<BrennenstuhlData> {
 public:
  void encode(RemoteTransmitData *dst, const BrennenstuhlData &data);
  optional<BrennenstuhlData> decode(RemoteReceiveData src);
  void dump(const BrennenstuhlData &data);
};

DECLARE_REMOTE_PROTOCOL(Brennenstuhl)

template<typename... Ts> class BrennenstuhlAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, code)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    BrennenstuhlData data{};
    data.code = this->code_.value(x...);
    BrennenstuhlProtocol().encode(dst, data);
  }
};

}  // namespace esphome::remote_base
