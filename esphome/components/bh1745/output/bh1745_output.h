#pragma once

#include "esphome/components/output/binary_output.h"
#include "esphome/core/helpers.h"

#include "../bh1745.h"

namespace esphome::bh1745 {

/// Drives the BH1745 INT pin, which switches the LEDs on breakout boards such as the Pimoroni one.
class BH1745InterruptPinOutput : public output::BinaryOutput, public Parented<BH1745Component> {
 protected:
  void write_state(bool state) override { this->parent_->set_interrupt_state(state); }
};

}  // namespace esphome::bh1745
