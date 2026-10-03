#pragma once

#include "remote_base.h"

#include <vector>

namespace esphome::remote_base {

struct AEHAData {
  uint16_t address;
  std::vector<uint8_t> data;

  bool operator==(const AEHAData &rhs) const { return address == rhs.address && data == rhs.data; }
};

class AEHAProtocol : public RemoteProtocol<AEHAData> {
 public:
  void encode(RemoteTransmitData *dst, const AEHAData &data);
  optional<AEHAData> decode(RemoteReceiveData src);
  void dump(const AEHAData &data);

 private:
  std::string format_data_(const std::vector<uint8_t> &data);
};

DECLARE_REMOTE_PROTOCOL(AEHA)

template<typename... Ts> class AEHAAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  void set_data_template(std::vector<uint8_t> (*func)(Ts...)) { this->data_.set_template(func); }
  void set_data_static(const uint8_t *data, int16_t len) { this->data_.set_static(data, len); }
  TEMPLATABLE_VALUE(uint32_t, carrier_frequency);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    AEHAData data{};
    data.address = this->address_.value(x...);
    data.data = this->data_.value(x...);
    dst->set_carrier_frequency(this->carrier_frequency_.value(x...));
    AEHAProtocol().encode(dst, data);
  }

 protected:
  TemplatableBytes<Ts...> data_;
};

}  // namespace esphome::remote_base
