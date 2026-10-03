#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome::remote_base {

struct MirageData {
  std::vector<uint8_t> data;

  bool operator==(const MirageData &rhs) const { return data == rhs.data; }
};

class MirageProtocol : public RemoteProtocol<MirageData> {
 public:
  void encode(RemoteTransmitData *dst, const MirageData &data);
  optional<MirageData> decode(RemoteReceiveData src);
  void dump(const MirageData &data);

 protected:
  void encode_byte_(RemoteTransmitData *dst, uint8_t item);
};

DECLARE_REMOTE_PROTOCOL(Mirage)

template<typename... Ts> class MirageAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  void set_code_template(std::vector<uint8_t> (*func)(Ts...)) { this->code_.set_template(func); }
  void set_code_static(const uint8_t *code, int16_t len) { this->code_.set_static(code, len); }

  void encode(RemoteTransmitData *dst, Ts... x) override {
    MirageData data{};
    data.data = this->code_.value(x...);
    MirageProtocol().encode(dst, data);
  }

 protected:
  TemplatableBytes<Ts...> code_;
};

}  // namespace esphome::remote_base
