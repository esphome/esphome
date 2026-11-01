#pragma once

#include <memory>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "datalink.h"

namespace esphome::opentherm42 {

// OpenTherm 4.2 master, implementing the OT/+ (OpenTherm/plus) digital protocol only -- §1.2: the
// two-microprocessor variant, as opposed to OT/- (OpenTherm/Lite), the PWM-signal variant for
// analogue-only products. §6 notes OT/- has been demoted to legacy/reference-only status since v4.1
// and should not be used in new designs -- this component never implements it. Talks directly to a
// single boiler -- see the OpenTherm Protocol Specification v4.2, §4.3.2: this component implements
// the master role only, not the optional gateway (chained intermediate device) role.
//
// At this stage the hub only keeps the bus alive: it repeats the §5.2 Status exchange (id=0) and logs
// every frame it sends and receives.
class OpenTherm42Hub : public Component {
 public:
  void set_in_pin(InternalGPIOPin *in_pin) { this->in_pin_ = in_pin; }
  void set_out_pin(InternalGPIOPin *out_pin) { this->out_pin_ = out_pin; }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  // §4.3.1: minimum time between the end of one conversation and the start of the next.
  static constexpr uint32_t MASTER_WAIT_TIME_MS = 100;
  // §4.3.1: the legal boiler answering-time window is 20-400 ms from the end of the master's
  // transmission; 400 ms is the longest a compliant boiler is allowed to take.
  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 400;

  InternalGPIOPin *in_pin_{nullptr};
  InternalGPIOPin *out_pin_{nullptr};

  std::unique_ptr<OpenThermDataLink> datalink_;

  uint32_t last_conversation_end_ms_{0};
};

}  // namespace esphome::opentherm42
