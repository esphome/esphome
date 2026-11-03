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
  /// data may be a PROGMEM table
  void encode(RemoteTransmitData *dst, const uint8_t *data, size_t len);
  void encode(RemoteTransmitData *dst, const std::vector<uint8_t> &data) {
    this->encode(dst, data.data(), data.size());
  }
  optional<MirageData> decode(RemoteReceiveData src);
  void dump(const MirageData &data);

 protected:
  void encode_byte_(RemoteTransmitData *dst, uint8_t item);
};

DECLARE_REMOTE_PROTOCOL(Mirage)

}  // namespace esphome::remote_base
