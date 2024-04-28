#pragma once

#include "remote_base.h"
#include <vector>

namespace esphome {
namespace remote_base {

struct HaierData {
  std::vector<uint8_t> data;

  bool operator==(const HaierData &rhs) const { return data == rhs.data; }
};

class HaierProtocol : public RemoteProtocol<HaierData> {
 public:
  void encode(RemoteTransmitData *dst, const HaierData &data) override;
  optional<HaierData> decode(RemoteReceiveData src) override;
  void dump(const HaierData &data) override;

 protected:
  void encode_byte_(RemoteTransmitData *dst, uint8_t item);
};

DECLARE_REMOTE_PROTOCOL(Haier)

template<typename... Ts> class HaierAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, code)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    HaierData data{};
    data.data = this->code_.value(x...);
    HaierProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
