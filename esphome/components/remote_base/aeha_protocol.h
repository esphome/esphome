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
  void encode(RemoteTransmitData *dst, const AEHAData &data) {
    this->encode(dst, data.address, data.data.data(), data.data.size());
  }
  void encode(RemoteTransmitData *dst, uint16_t address, const uint8_t *data, size_t len);
  optional<AEHAData> decode(RemoteReceiveData src);
  void dump(const AEHAData &data);

 private:
  std::string format_data_(const std::vector<uint8_t> &data);
};

DECLARE_REMOTE_PROTOCOL(AEHA)

template<typename... Ts> class AEHAAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, address)
  TEMPLATABLE_BYTES(data)
  TEMPLATABLE_VALUE(uint32_t, carrier_frequency);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    const uint16_t address = this->address_.value(x...);
    dst->set_carrier_frequency(this->carrier_frequency_.value(x...));
    this->data_.visit(
        [dst, address](const uint8_t *data, size_t len) { AEHAProtocol().encode(dst, address, data, len); }, x...);
  }
};

}  // namespace esphome::remote_base
