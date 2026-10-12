#pragma once

#include "remote_base.h"

#include <cinttypes>

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
 private:
  void wakeup_(RemoteTransmitData *dst) const;
  void hw_sync_(RemoteTransmitData *dst) const;
  void sw_sync_(RemoteTransmitData *dst) const;

  // Helper functions for manchester encoding
  inline void zero_(RemoteTransmitData *dst) const;
  inline void one_(RemoteTransmitData *dst) const;

 public:
  void encode(RemoteTransmitData *dst, const SomfyRtsData &data);
  optional<SomfyRtsData> decode(RemoteReceiveData src);
  void dump(const SomfyRtsData &data);
};

DECLARE_REMOTE_PROTOCOL(SomfyRts)

}  // namespace esphome::remote_base
