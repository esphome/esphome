#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"

namespace esphome::beken_irda {

class BekenIrdaComponent final : public Component, public remote_base::RemoteReceiverBase {
 public:
  explicit BekenIrdaComponent(InternalGPIOPin *pin) : remote_base::RemoteReceiverBase(pin) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  static void irda_isr();
  void handle_isr_();
  void dispatch_full_(uint32_t code);
  void dispatch_repeat_();

  volatile uint32_t last_code_{0};
  // Rejected codes and IRDA errors, and codes overwritten before dispatch; reset by every loop pass.
  // Saturating, so a flood between passes reads as 255 rather than wrapping to zero
  volatile uint8_t error_count_{0};
  volatile uint8_t dropped_count_{0};
  // Bit fields keep the flags in one byte; the loop changes them only under InterruptLock
  volatile bool raw_ready_ : 1 {false};
  volatile bool repeat_ready_ : 1 {false};
  /// Repeats belong to the last frame the ISR read, so they are dropped after a rejected one
  volatile bool repeats_valid_ : 1 {false};
};

}  // namespace esphome::beken_irda
