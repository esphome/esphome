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

  /// Called from the IRDA interrupt handler.
  void handle_isr();

 protected:
  void dispatch_full_(uint32_t code);
  void dispatch_repeat_();

  uint32_t last_reject_{0};
  uint32_t last_error_{0};

  volatile uint32_t last_code_{0};
  volatile uint32_t reject_count_{0};
  volatile uint32_t error_count_{0};
  volatile bool raw_ready_{false};
  volatile bool repeat_ready_{false};
};

}  // namespace esphome::beken_irda
