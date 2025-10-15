#pragma once

#include "remote_base.h"

#include <vector>

namespace esphome {
namespace remote_base {

struct AEHAData {
  uint16_t address;
  std::vector<uint8_t> data;

  bool operator==(const AEHAData &rhs) const { return address == rhs.address && data == rhs.data; }
};

class AEHAProtocol : public RemoteProtocol<AEHAData> {
 public:
  void encode(RemoteTransmitData *dst, const AEHAData &data) override;
  optional<AEHAData> decode(RemoteReceiveData src) override;
  void dump(const AEHAData &data) override;

 private:
  std::string format_data_(const std::vector<uint8_t> &data);
};

DECLARE_REMOTE_PROTOCOL(AEHA)

template<typename... Ts> class AEHAAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)
  TEMPLATABLE_VALUE(uint32_t, carrier_frequency);

  void set_data(const std::vector<uint8_t> &data) { data_ = data; }
  void encode(RemoteTransmitData *dst, Ts... x) override {
    AEHAData data{};
    data.address = this->address_.value(x...);
    data.data = this->data_.value(x...);
    dst->set_carrier_frequency(this->carrier_frequency_.value(x...));
    AEHAProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
