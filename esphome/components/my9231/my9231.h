#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::my9231 {

/// MY9231 float output component.
class MY9231OutputComponent final : public Component {
 public:
  /// @param pwm_amounts storage for num_channels duty values, owned by the caller for the life of the program.
  MY9231OutputComponent(GPIOPin *pin_di, GPIOPin *pin_dcki, uint16_t *pwm_amounts, uint16_t num_channels,
                        uint8_t num_chips, uint8_t bit_depth)
      : pin_di_(pin_di),
        pin_dcki_(pin_dcki),
        pwm_amounts_(pwm_amounts),
        num_channels_(num_channels),
        num_chips_(num_chips),
        bit_depth_(bit_depth) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  /// Send the new duty values, then sleep until write_channel() changes one.
  void loop() override;

  void write_channel(uint16_t channel, float state);

 protected:
  uint16_t get_max_amount_() const { return (uint32_t(1) << this->bit_depth_) - 1; }

  void init_chips_(uint8_t command);
  void write_word_(uint16_t value, uint8_t bits);
  void send_di_pulses_(uint8_t count);
  void send_dcki_pulses_(uint8_t count);

  GPIOPin *const pin_di_;
  GPIOPin *const pin_dcki_;
  uint16_t *const pwm_amounts_;
  const uint16_t num_channels_;
  const uint8_t num_chips_;
  const uint8_t bit_depth_;
};

}  // namespace esphome::my9231
