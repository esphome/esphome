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
// .state (inherited from number::Number) is what's *displayed*. For ids with no read-back (no
// corresponding READ-DATA data-id defined by the spec), it's republished from whatever the boiler's
// WRITE-ACK most recently echoed back (see hub.cpp's handle_response_()), per §4.4.2/§5.1's convention
// that the echo reflects what was actually accepted (the boiler may have clamped it) -- though real
// hardware has been observed not honoring this convention for at least one id, which is why ids with a
// real READ-DATA counterpart instead update .state from that read, never from the WRITE-ACK (see hub.h's
// RequestKind::DHW_SETPOINT_READ-style comments).
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
