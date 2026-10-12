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
  void encode(RemoteTransmitData *dst, const MirageData &data) {
    this->encode(dst, data.data.data(), data.data.size());
  }
  void encode(RemoteTransmitData *dst, const uint8_t *data, size_t len);
  optional<MirageData> decode(RemoteReceiveData src);
  void dump(const MirageData &data);

 protected:
  void encode_byte_(RemoteTransmitData *dst, uint8_t item);
};

DECLARE_REMOTE_PROTOCOL(Mirage)

template<typename... Ts> class MirageAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_BYTES(code)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    this->code_.visit([dst](const uint8_t *data, size_t len) { MirageProtocol().encode(dst, data, len); }, x...);
  }
};

}  // namespace esphome::remote_base
