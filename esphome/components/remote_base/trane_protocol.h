#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

// Dev note: The protocol uses two "words", one 35-bit and one 32-bit. I tried using a 64-bit int
// to represent the first one, but for whatever reason the code would always truncate it to 32 bit
// before I could even pass it to the IR encoding. I decided to split up the 3 bits that represent
// A/C operation mode so that ints are 32-bit or smaller, and this worked.

struct TraneData {
  uint8_t mode;
  uint32_t trane_data_1;
  uint32_t trane_data_2;
};

class TraneProtocol : public RemoteProtocol<TraneData>{
 public:
  void encode(RemoteTransmitData *dst, const TraneData &data) override;
  optional<TraneData> decode(RemoteReceiveData src) override;
  void dump(const TraneData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Trane)
template<typename... Ts> class TraneAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, trane_mode);
  TEMPLATABLE_VALUE(uint32_t, trane_data_1);
  TEMPLATABLE_VALUE(uint32_t, trane_data_2);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    TraneData data{};
    data.mode = this->trane_mode_.value(x...);
    data.trane_data_1 = this->trane_data_1_.value(x...);
    data.trane_data_2 = this->trane_data_2_.value(x...);
    TraneProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
