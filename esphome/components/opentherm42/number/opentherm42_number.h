#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/number/number.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// A number entity whose value is written to the boiler. Like OpenTherm42Switch, the write is
// fire-and-forget -- publish_state() happens immediately, there's no hardware ack to wait for.
// Always restores its last value from flash on boot, falling back to initial_value_ on first boot or
// a corrupt/missing preference -- see CLAUDE.md's write-only-entity rule: the master must be guaranteed
// to display the value it last successfully wrote to the boiler, not silently revert to a fixed default.
//
// .state (inherited from number::Number) is what's *displayed*: it's set exactly twice -- optimistically
// in control()/setup(), and on an explicit rejection (DATA_INVALID/UNKNOWN_DATA_ID), which invalidates it
// back to Unknown (see hub.cpp's handle_response_()). A successful WRITE-ACK never touches it: §4.4.2's
// convention is that the echoed value reflects what the boiler actually accepted (it may have clamped
// it), but real hardware has been observed acking with an echo of 0 (or some other unrelated value)
// regardless of what was actually written, on more than one id -- trusting it produced a phantom "reverts
// to 0" display bug with no correlation to what the boiler is actually doing. Since these ids have no
// READ-DATA counterpart to independently verify against (see OpenTherm42SensorFeedNumber and the
// RequestKind::DHW_SETPOINT-style comments in hub.h for ids that do), there's no trustworthy alternative
// source of truth -- the last commanded value is the best available approximation.
//
// What's *sent* is a completely separate concern, deliberately not stored here: every control()/setup()
// value gets pushed straight to the hub via set_write_value(id, ...), which build_next_request_() reads
// back when building the outgoing WRITE_DATA frame. hub and id are both required and never change after
// construction (see CLAUDE.md's "Constructor parameters vs setters" rule) -- id is also what lets the
// hub route the pushed value to the right internal field without this class exposing its concrete type
// to hub.h (hub.h must stay buildable for configs that don't use any opentherm42 number at all, so it
// can only ever hold the generic number::Number*, never OpenTherm42Number* -- see set_write_value()'s
// declaration comment).
class OpenTherm42Number : public number::Number, public Component {
 public:
  OpenTherm42Number(OpenTherm42Hub *hub, uint8_t id) : hub_(hub), id_(id) {}

  void set_initial_value(float initial_value) { this->initial_value_ = initial_value; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  OpenTherm42Hub *hub_;
  uint8_t id_;
  float initial_value_{0};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::opentherm42
