#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::remko_ar715 {

// Remko AR-715 IR remote: 52-bit pulse-distance frame, full state in every command.
// IRremoteESP8266 calls it the ClimaButler protocol after a rebrand; Remko is the manufacturer.
// Protocol analysis: https://github.com/crankyoldgit/IRremoteESP8266/issues/1812
//
// Supported devices:
//   - Remko RKL series (RKL 490, 491, 494, 495) with AR-715 remote
//   - Fischer ClimaButler RCS-SD43UAI / RCS-SD43UWI
//   - TROTEC PAC 4600
//   - Novamatic CL 990 / CL 1590
//   - Rexair C15000N
//   - freecom RCS-SD43UAI / RCS-SD43UWI

static constexpr float TEMP_MIN = 16.0f;
static constexpr float TEMP_MAX = 30.0f;

class RemkoAr715Climate final : public climate_ir::ClimateIR {
 public:
  RemkoAr715Climate()
      : climate_ir::ClimateIR(TEMP_MIN, TEMP_MAX,
                              1.0f,  // temperature step
                              true,  // supports dry
                              true,  // supports fan only
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

 protected:
  void transmit_state() override;
};

}  // namespace esphome::remko_ar715
