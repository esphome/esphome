#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/remote_base/aeha_protocol.h"

namespace esphome::mistral_ir {

// Temperature range, Celsius
constexpr uint8_t MISTRAL_TEMP_MIN = 16;
constexpr uint8_t MISTRAL_TEMP_MAX = 30;

constexpr uint16_t MISTRAL_ADDRESS = 0x322C;

/* Reverse-engineered protocol for a "Mistral" (Belgian brand, model MHFE1126A / MR112C) split AC unit
   from the 2000-2010 era, likely a rebadged OEM design. It is carried over AEHA IR timing (address
   0x322C) but does not match any protocol already implemented in ESPHome, so it is decoded/encoded
   directly here rather than through the generic AEHA helper.

   Frame layout (12 bytes): 56 xx 08 xx xx xx xx 00 E8 00 60 xx
     byte[1] / byte[6] -- fan speed (paired with byte[0]/byte[2]/byte[7]/byte[8]/byte[10], all fixed)
     byte[3]           -- power: 0x20 on, 0x00 off
     byte[4]           -- mode
     byte[5]           -- target temperature (bit-reversed offset from 0x9F)
     byte[11]          -- checksum: bit-reverse the sum of bit-reversed bytes 1, 3, 4, 5, 6, 8
*/
class MistralIR : public climate_ir::ClimateIR {
 public:
  MistralIR()
      : climate_ir::ClimateIR(MISTRAL_TEMP_MIN, MISTRAL_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH}) {}

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  // Reused across transmit_state() calls to avoid a heap allocation on every send; the address and the
  // 12-byte data buffer size never change, only the byte contents are overwritten in place.
  remote_base::AEHAData frame_{MISTRAL_ADDRESS, std::vector<uint8_t>(12)};
};

}  // namespace esphome::mistral_ir
