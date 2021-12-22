#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct JVCData {
  uint32_t data;

  bool operator==(const JVCData &rhs) const { return data == rhs.data; }
};

class JVCProtocol : public RemoteProtocol<JVCData> {
 public:
  void encode(RemoteTransmitData *dst, const JVCData &data) override;
  optional<JVCData> decode(RemoteReceiveData src) override;
  void dump(const JVCData &data) override;
};

DECLARE_REMOTE_PROTOCOL(JVC)

template<typename... Ts> class JVCAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, data)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    JVCData data{};
    data.data = this->data_.value(x...);
    JVCProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
