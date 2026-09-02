#pragma once

#include <memory>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "datalink.h"

namespace esphome::opentherm42 {

// OpenTherm 4.2 master. Talks directly to a single boiler -- see the OpenTherm Protocol
// Specification v4.2, §4.3.2: this component implements the master role only, not the optional
// gateway (chained intermediate device) role.
class OpenTherm42Hub : public Component {
 public:
  void set_in_pin(InternalGPIOPin *in_pin) { this->in_pin_ = in_pin; }
  void set_out_pin(InternalGPIOPin *out_pin) { this->out_pin_ = out_pin; }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  InternalGPIOPin *in_pin_{nullptr};
  InternalGPIOPin *out_pin_{nullptr};

  std::unique_ptr<OpenThermDataLink> datalink_;
};

}  // namespace esphome::opentherm42
