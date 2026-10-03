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
  void encode(RemoteTransmitData *dst, const HaierData &data);
  optional<HaierData> decode(RemoteReceiveData src);
  void dump(const HaierData &data);

 protected:
  void encode_byte_(RemoteTransmitData *dst, uint8_t item);
};

DECLARE_REMOTE_PROTOCOL(Haier)

template<typename... Ts> class HaierAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  void set_code_template(std::vector<uint8_t> (*func)(Ts...)) { this->code_.set_template(func); }
  void set_code_static(const uint8_t *code, int16_t len) { this->code_.set_static(code, len); }

  void encode(RemoteTransmitData *dst, Ts... x) override {
    HaierData data{};
    data.data = this->code_.value(x...);
    HaierProtocol().encode(dst, data);
  }

 protected:
  TemplatableBytes<Ts...> code_;
};

}  // namespace esphome::remote_base
