#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::tlc59208f {

// 0*: Group dimming, 1: Group blinking
inline constexpr uint8_t TLC59208F_MODE2_DMBLNK = (1 << 5);
// 0*: Output change on Stop command, 1: Output change on ACK
inline constexpr uint8_t TLC59208F_MODE2_OCH = (1 << 3);
// 0*: WDT disabled, 1: WDT enabled
inline constexpr uint8_t TLC59208F_MODE2_WDTEN = (1 << 2);
// WDT timeouts
inline constexpr uint8_t TLC59208F_MODE2_WDT_5MS = (0 << 0);
inline constexpr uint8_t TLC59208F_MODE2_WDT_15MS = (1 << 0);
inline constexpr uint8_t TLC59208F_MODE2_WDT_25MS = (2 << 0);
inline constexpr uint8_t TLC59208F_MODE2_WDT_35MS = (3 << 0);

/// TLC59208F float output component.
class TLC59208FOutput final : public Component, public i2c::I2CDevice {
 public:
  TLC59208FOutput(uint8_t mode = TLC59208F_MODE2_OCH) : mode_(mode) {}

  void register_channel(uint8_t channel);
  void write_channel(uint8_t channel, float state);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;

 protected:
  uint8_t mode_;

  uint8_t min_channel_{0xFF};
  uint8_t max_channel_{0x00};
  uint8_t pwm_amounts_[256] = {
      0,
  };
  bool update_{true};
};

}  // namespace esphome::tlc59208f
