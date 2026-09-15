#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// §5.3.6 Class 6, IDs 11/89/106: a single transparent-boiler-parameter slot. hub and slot_index are
// both required and never change after construction (see CLAUDE.md's "Constructor parameters vs
// setters" rule): slot_index identifies this instance's entry in the hub's tsp_slots_, assigned by
// OpenTherm42Hub::add_tsp_slot() when this number is registered.
class OpenTherm42TspNumber : public number::Number, public Component {
 public:
  OpenTherm42TspNumber(OpenTherm42Hub *hub, size_t slot_index) : hub_(hub), slot_index_(slot_index) {}

 protected:
  // Queues the write; state is only published once the boiler's response (read or write-ack) comes
  // back, since §5.3.6 warns the boiler may silently clamp an out-of-range TSP-value.
  void control(float value) override { this->hub_->write_tsp(this->slot_index_, static_cast<uint8_t>(value)); }

  OpenTherm42Hub *hub_;
  size_t slot_index_;
};

}  // namespace esphome::opentherm42
