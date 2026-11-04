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
  /// Rejected codes and IRDA errors since the loop last reported them
  // Reset by every loop pass, so a byte each is enough
  volatile uint8_t error_count_{0};
  volatile uint8_t dropped_count_{0};
  volatile bool raw_ready_{false};
  volatile bool repeat_ready_{false};
};

}  // namespace esphome::beken_irda
