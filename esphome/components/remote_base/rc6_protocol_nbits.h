#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct RC6NbitsData {
  uint8_t nbits;
  uint64_t code;

  bool operator==(const RC6NbitsData &rhs) const { return nbits == rhs.nbits && code == rhs.code; }
};

class RC6NbitsProtocol : public RemoteProtocol<RC6NbitsData> {
 public:
  void encode(RemoteTransmitData *dst, const RC6NbitsData &data) override;
  optional<RC6NbitsData> decode(RemoteReceiveData src) override;
  void dump(const RC6NbitsData &data) override;
};

DECLARE_REMOTE_PROTOCOL(RC6Nbits)

template<typename... Ts> class RC6NbitsAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, nbits)
  TEMPLATABLE_VALUE(uint32_t, code)

  void encode(RemoteTransmitData *dst, Ts... x) {
    RC6NbitsData data{};
    data.nbits = this->nbits_.value(x...);
    data.code = this->code_.value(x...);
    RC6NbitsProtocol().encode(dst, data);
  }

};

}  // namespace remote_base
}  // namespace esphome
