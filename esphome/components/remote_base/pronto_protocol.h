#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct ProntoData {
  std::string data;

  bool operator==(const ProntoData &rhs) const { return data == rhs.data; }
};

class ProntoProtocol : public RemoteProtocol<ProntoData> {
 private:
  void send_pronto_(RemoteTransmitData *dst, const std::vector<uint16_t> &data);
  void send_pronto_(RemoteTransmitData *dst, const std::string &str);

 public:
  void encode(RemoteTransmitData *dst, const ProntoData &data) override;
  optional<ProntoData> decode(RemoteReceiveData src) override;
  void dump(const ProntoData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Pronto)

template<typename... Ts> class ProntoAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, data)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    ProntoData data{};
    data.data = this->data_.value(x...);
    ProntoProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
