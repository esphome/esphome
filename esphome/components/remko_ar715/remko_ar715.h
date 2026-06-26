#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::remko_ar715 {

// Remko AR-715 IR Remote protocol
// Used by: Remko RKL series, Fischer ClimaButler RCS-SD43, TROTEC PAC 4600,
//          Novamatic CL 990/1590, Rexair C15000N, freecom RCS-SD43
//
// Protocol analysis: https://github.com/crankyoldgit/IRremoteESP8266/issues/1812
// 52-bit pulse-distance encoding, all state sent per command.

// IR Timing (from ir_ClimaButler.cpp, IRremoteESP8266)
const uint16_t REMKO_AR715_BIT_MARK = 511;     // uSeconds
const uint16_t REMKO_AR715_HDR_SPACE = 3492;   // uSeconds
const uint16_t REMKO_AR715_ONE_SPACE = 1540;   // uSeconds
const uint16_t REMKO_AR715_ZERO_SPACE = 548;   // uSeconds
const uint32_t REMKO_AR715_FREQUENCY = 38000;  // Hz
const uint8_t REMKO_AR715_BITS = 52;

// Temperature range
const float REMKO_AR715_TEMP_MIN = 16.0f;
const float REMKO_AR715_TEMP_MAX = 30.0f;

// Protocol nibble constants (52-bit value, 13 nibbles MSB->LSB)
// N12=0x8 (header), N11=power, N10-N5=timer(0), N4=swing, N3+N2=mode/fan, N1=temp, N0=checksum

// Power (N11)
const uint8_t REMKO_AR715_POWER_ON = 0x3;
const uint8_t REMKO_AR715_POWER_OFF = 0x0;

// Swing (N4)
const uint8_t REMKO_AR715_SWING_OFF = 0x7;
const uint8_t REMKO_AR715_SWING_ON = 0x5;

// Mode/Fan (N3, N2)
const uint8_t REMKO_AR715_FAN_AUTO_N3 = 0x8, REMKO_AR715_FAN_AUTO_N2 = 0x0;      // Fuzzy/Auto
const uint8_t REMKO_AR715_FAN_LOW_N3 = 0xA, REMKO_AR715_FAN_LOW_N2 = 0x2;        // Low
const uint8_t REMKO_AR715_FAN_MEDIUM_N3 = 0x9, REMKO_AR715_FAN_MEDIUM_N2 = 0x1;  // Medium
const uint8_t REMKO_AR715_FAN_HIGH_N3 = 0x8, REMKO_AR715_FAN_HIGH_N2 = 0x8;      // High
const uint8_t REMKO_AR715_MODE_DRY_N3 = 0xA, REMKO_AR715_MODE_DRY_N2 = 0x4;      // Dry
const uint8_t REMKO_AR715_MODE_FAN_N3 = 0xA, REMKO_AR715_MODE_FAN_N2 = 0x3;      // Fan-only

class RemkoAr715Climate : public climate_ir::ClimateIR {
 public:
  RemkoAr715Climate()
      : climate_ir::ClimateIR(REMKO_AR715_TEMP_MIN, REMKO_AR715_TEMP_MAX,
                              1.0f,  // temperature step
                              true,  // supports dry
                              true,  // supports fan only
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  // Note: supports_cool and supports_heat are accepted by the base class schema
  // but have no effect here. The traits() override explicitly excludes
  // CLIMATE_MODE_HEAT and CLIMATE_MODE_HEAT_COOL because the AR-715 remote
  // does not support these modes.
  // TODO: remove this override once https://github.com/esphome/esphome/pull/16786 is merged.
  climate::ClimateTraits traits() override {
    auto traits = climate_ir::ClimateIR::traits();
    traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_DRY,
        climate::CLIMATE_MODE_FAN_ONLY,
    });
    return traits;
  }

 protected:
  /// Transmit the current climate state via IR.
  void transmit_state() override;

 private:
  /// Build the 52-bit protocol word from the given parameters.
  uint64_t build_code_(bool power, bool swing, uint8_t n3, uint8_t n2, uint8_t temp);

  /// Send a 52-bit protocol word via the remote transmitter.
  void send_(uint64_t code);
};

}  // namespace esphome::remko_ar715
