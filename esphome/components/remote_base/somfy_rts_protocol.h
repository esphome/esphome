#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct SomfyRtsData {
  uint32_t address;
  uint16_t rolling_code;
  uint8_t key;
  uint8_t control;
  bool control_in_key;
};

// Transmit only: there is no decoder yet, so no binary sensor, trigger or dumper is registered.
class SomfyRtsProtocol : public RemoteProtocol<SomfyRtsData> {
 public:
  void encode(RemoteTransmitData *dst, const SomfyRtsData &data);
};

}  // namespace esphome::remote_base
