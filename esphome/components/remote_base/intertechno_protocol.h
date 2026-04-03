#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

struct IntertechnoData {
  std::string code;

  bool operator==(const IntertechnoData &rhs) const { return code == rhs.code; }
};

class IntertechnoProtocol : public RemoteProtocol<IntertechnoData> {
 public:
  void encode(RemoteTransmitData *dst, const IntertechnoData &data) override;
  optional<IntertechnoData> decode(RemoteReceiveData src) override;
  void dump(const IntertechnoData &data) override;

 private:
  void encode_0(RemoteTransmitData *dst);
  void encode_1(RemoteTransmitData *dst);
  void encode_F(RemoteTransmitData *dst);
};

DECLARE_REMOTE_PROTOCOL(Intertechno)

template<typename... Ts> class IntertechnoAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, code)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    IntertechnoData data{};
    data.code = this->code_.value(x...);
    IntertechnoProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
