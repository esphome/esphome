#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// §5.3.6 Class 6, IDs 11/89/106: a single transparent-boiler-parameter slot. hub is required and never
// changes after construction (see CLAUDE.md's "Constructor parameters vs setters" rule). The hub finds
// this instance's entry in its tsp_slots_ by pointer, registered with OpenTherm42Hub::add_tsp_slot().
class OpenTherm42TspNumber : public number::Number, public Component {
 public:
  explicit OpenTherm42TspNumber(OpenTherm42Hub *hub) : hub_(hub) {}

 protected:
  // Queues the write; state is only published once the boiler's response (read or write-ack) comes
  // back, since §5.3.6 warns the boiler may silently clamp an out-of-range TSP-value.
  void control(float value) override { this->hub_->write_tsp(this, static_cast<uint8_t>(value)); }

  OpenTherm42Hub *hub_;
};

}  // namespace esphome::opentherm42
