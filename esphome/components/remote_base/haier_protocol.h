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
  /// data may be a PROGMEM table
  void encode(RemoteTransmitData *dst, const uint8_t *data, size_t len);
  void encode(RemoteTransmitData *dst, const std::vector<uint8_t> &data) {
    this->encode(dst, data.data(), data.size());
  }
  optional<HaierData> decode(RemoteReceiveData src);
  void dump(const HaierData &data);

 protected:
  void encode_byte_(RemoteTransmitData *dst, uint8_t item);
};

DECLARE_REMOTE_PROTOCOL(Haier)

}  // namespace esphome::remote_base
