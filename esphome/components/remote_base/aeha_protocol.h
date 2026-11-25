#pragma once

#include "remote_base.h"

#include <vector>

namespace esphome::remote_base {

/// An AEHA frame carries at most 35 data bytes
static constexpr size_t AEHA_MAX_DATA_BYTES = 35;

struct AEHAData {
  uint16_t address;
  RemoteBytes<AEHA_MAX_DATA_BYTES> data;

  bool operator==(const AEHAData &rhs) const { return address == rhs.address && data == rhs.data; }
};

class AEHAProtocol : public RemoteProtocol<AEHAData> {
 public:
  void encode(RemoteTransmitData *dst, const AEHAData &data) {
    this->encode(dst, data.address, data.data.data(), data.data.size());
  }
  /// data may be a PROGMEM table
  void encode(RemoteTransmitData *dst, uint16_t address, const uint8_t *data, size_t len);
  void encode(RemoteTransmitData *dst, uint16_t address, const std::vector<uint8_t> &data) {
    this->encode(dst, address, data.data(), data.size());
  }
  optional<AEHAData> decode(RemoteReceiveData src);
  void dump(const AEHAData &data);
};

DECLARE_REMOTE_PROTOCOL(AEHA)

}  // namespace esphome::remote_base
