#pragma once

#include <memory>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "datalink.h"

namespace esphome::opentherm42 {

// §5.2: which mandatory conversation is currently in flight, so handle_response_() knows how to
// interpret the reply. Class-owned data-ids (Class 1's ID 0/1, Class 2's ID 3, ...) get their own
// scheduling as their commits add them; this only covers what §5.2 requires every master to do
// before any class-specific entity exists to drive it.
enum class RequestKind : uint8_t {
  // §5.3.2 Class 2, ID 3: boiler configuration flags + boiler MemberID code. Read once at startup
  // (§5.2.1: "Must sent message with READ_DATA (at least at start up)").
  BOILER_CONFIG,
  // §5.3.1 Class 1, ID 0: the master/boiler status exchange -- the protocol's mandatory heartbeat.
  // Master status bits are all clear (0) until Class 1 (Commit 4) wires up real entities for them.
  STATUS,
  // §5.3.1 Class 1, ID 1: control setpoint. Held at 0 (no demand) until Class 1 wires up a real
  // setpoint source.
  CONTROL_SETPOINT,
};

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
  // §4.3.1: minimum time between the end of one conversation and the start of the next.
  static constexpr uint32_t MASTER_WAIT_TIME_MS = 100;
  // §4.3.1: the legal boiler answering-time window is 20-400 ms from the end of the master's
  // transmission; 400 ms is the longest a compliant boiler is allowed to take.
  static constexpr uint32_t RESPONSE_TIMEOUT_MS = 400;

  // Builds the next mandatory request to send, advancing the round-robin/startup scheduling state.
  Frame build_next_request_();
  // Interprets a received frame according to which request it answers; logs and discards it if the
  // boiler replied with a message type §5.2.1 forbids for a mandatory data-id.
  void handle_response_(const Frame &frame);

  InternalGPIOPin *in_pin_{nullptr};
  InternalGPIOPin *out_pin_{nullptr};

  std::unique_ptr<OpenThermDataLink> datalink_;

  uint32_t last_conversation_end_ms_{0};
  RequestKind pending_request_kind_{RequestKind::BOILER_CONFIG};
  bool boiler_config_read_{false};
  bool next_is_status_{true};

  // Raw values from the mandatory conversations -- exposed as real entities once Class 1 (Commit 4)
  // and Class 2 (Commit 5) land.
  uint8_t boiler_status_{0};
  uint8_t boiler_config_flags_{0};
  uint8_t boiler_member_id_code_{0};
};

}  // namespace esphome::opentherm42
