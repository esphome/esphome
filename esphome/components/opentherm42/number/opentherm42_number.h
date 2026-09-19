#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/number/number.h"

namespace esphome::opentherm42 {

// A number entity whose value is written to the boiler. Like OpenTherm42Switch, the write is
// fire-and-forget -- publish_state() happens immediately, there's no hardware ack to wait for.
// Always restores its last value from flash on boot, falling back to initial_value_ on first boot or
// a corrupt/missing preference -- see CLAUDE.md's write-only-entity rule: the master must be guaranteed
// to display the value it last successfully wrote to the boiler, not silently revert to a fixed default.
//
// Two independent value fields, deliberately not conflated:
//  - .state (inherited from number::Number) is what's *displayed*. For ids with no read-back (no
//    corresponding READ-DATA data-id defined by the spec), it's republished from whatever the
//    boiler's WRITE-ACK most recently echoed back (see hub.cpp's handle_response_()), per §4.4.2/§5.1's
//    convention that the echo reflects what was actually accepted (the boiler may have clamped it) --
//    though real hardware has been observed not honoring this convention for at least one id, which is
//    why ids with a real READ-DATA counterpart instead update .state from that read, never from the
//    WRITE-ACK (see hub.h's RequestKind::DHW_SETPOINT_READ-style comments).
//  - write_value_ is what's *sent*: the value the user (control()) or flash/initial_value_ (setup())
//    most recently commanded. hub.cpp's build_next_request_() must read write_value(), never .state,
//    when building the outgoing WRITE_DATA frame -- otherwise a boiler that echoes back anything other
//    than what was requested (a clamp, or one that silently ignores the write and echoes its own
//    unrelated value) permanently corrupts .state, and the wrong value gets re-sent forever.
class OpenTherm42Number : public number::Number, public Component {
 public:
  void set_initial_value(float initial_value) { this->initial_value_ = initial_value; }
  // The value most recently commanded -- see the class comment above for why build_next_request_()
  // must read this, not .state, to build the outgoing write.
  float write_value() const { return this->write_value_; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  float initial_value_{0};
  float write_value_{0};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::opentherm42
