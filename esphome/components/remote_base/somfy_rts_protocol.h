#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct SomfyRtsData {
  uint8_t key;
  uint8_t control;
  uint8_t checksum;
  uint16_t rolling_code;
  uint32_t address;
  bool control_in_key;

  bool operator==(const SomfyRtsData &rhs) const {
    return key == rhs.key && control == rhs.control && checksum == rhs.checksum && rolling_code == rhs.rolling_code &&
           address == rhs.address;
  }
};

class SomfyRtsProtocol : public RemoteProtocol<SomfyRtsData> {
 public:
  void encode(RemoteTransmitData *dst, const SomfyRtsData &data);
  optional<SomfyRtsData> decode(RemoteReceiveData src);
  void dump(const SomfyRtsData &data);
};

DECLARE_REMOTE_PROTOCOL(SomfyRts)

}  // namespace esphome::remote_base
