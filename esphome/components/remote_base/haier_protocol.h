#pragma once

#include "remote_base.h"
#include <vector>

namespace esphome::remote_base {

struct HaierData {
  std::vector<uint8_t> data;

  bool operator==(const HaierData &rhs) const { return data == rhs.data; }
};

class HaierProtocol : public RemoteProtocol<HaierData> {
 public:
  void encode(RemoteTransmitData *dst, const HaierData &data) { this->encode(dst, data.data.data(), data.data.size()); }
  void encode(RemoteTransmitData *dst, const uint8_t *data, size_t len);
  optional<HaierData> decode(RemoteReceiveData src);
  void dump(const HaierData &data);

 protected:
  void encode_byte_(RemoteTransmitData *dst, uint8_t item);
};

DECLARE_REMOTE_PROTOCOL(Haier)

template<typename... Ts> class HaierAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_BYTES(code)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    this->code_.visit([dst](const uint8_t *data, size_t len) { HaierProtocol().encode(dst, data, len); }, x...);
  }
};

}  // namespace esphome::remote_base
