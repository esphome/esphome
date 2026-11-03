#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace esphome::fusb302b {

static constexpr uint8_t PD_MAX_NUM_DATA_OBJECTS = 7;
// Standard Power Range fixed supplies, which are the only ones this sink negotiates
static constexpr uint8_t MIN_REQUEST_VOLTAGE = 5;
static constexpr uint8_t MAX_REQUEST_VOLTAGE = 20;

enum PdSpecRevision : uint8_t {
  PD_SPEC_REV_1 = 0,
  PD_SPEC_REV_2 = 1,
  PD_SPEC_REV_3 = 2,
};

enum PdDataMsgType : uint8_t {
  PD_DATA_SOURCE_CAP = 0x01,
  PD_DATA_REQUEST = 0x02,
  PD_DATA_SINK_CAP = 0x04,
};

enum PdControlMsgType : uint8_t {
  PD_CNTRL_GOODCRC = 0x01,
  PD_CNTRL_ACCEPT = 0x03,
  PD_CNTRL_REJECT = 0x04,
  PD_CNTRL_PING = 0x05,
  PD_CNTRL_PS_RDY = 0x06,
  PD_CNTRL_GET_SOURCE_CAP = 0x07,
  PD_CNTRL_GET_SINK_CAP = 0x08,
  PD_CNTRL_WAIT = 0x0C,
  PD_CNTRL_SOFT_RESET = 0x0D,
  PD_CNTRL_NOT_SUPPORTED = 0x10,
};

enum PdPdoType : uint8_t {
  PD_PDO_TYPE_FIXED_SUPPLY = 0,
  PD_PDO_TYPE_BATTERY = 1,
  PD_PDO_TYPE_VARIABLE_SUPPLY = 2,
  PD_PDO_TYPE_AUGMENTED = 3,
};

enum class PdState : uint8_t {
  PD_STATE_DISCONNECTED,
  /// Negotiation is over without a PD contract (no capabilities, or the request was not accepted); the source
  /// supplies Type-C default power.
  PD_STATE_PD_TIMEOUT,
  PD_STATE_DEFAULT_CONTRACT,
  PD_STATE_TRANSITION,
  PD_STATE_EXPLICIT_CONTRACT,
  PD_STATE_ERROR,
};

/// Returns true for the states in which a source is attached and supplying power.
inline bool is_connected_state(PdState state) {
  return state == PdState::PD_STATE_DEFAULT_CONTRACT || state == PdState::PD_STATE_TRANSITION ||
         state == PdState::PD_STATE_EXPLICIT_CONTRACT || state == PdState::PD_STATE_PD_TIMEOUT;
}

struct PdContract {
  PdPdoType type{PD_PDO_TYPE_FIXED_SUPPLY};
  uint16_t max_v{0};  // 50 mV units
  uint16_t max_i{0};  // 10 mA units

  bool operator==(const PdContract &other) const {
    return this->max_v == other.max_v && this->max_i == other.max_i && this->type == other.type;
  }
  bool operator!=(const PdContract &other) const { return !(*this == other); }
};

// Type-C default power: 5 V (100 x 50 mV) at 500 mA (50 x 10 mA)
static constexpr PdContract DEFAULT_CONTRACT{PD_PDO_TYPE_FIXED_SUPPLY, 100, 50};

/// A contract packed into one word so it can be shared between tasks as an atomic.
/// 0 means there is no contract (nothing attached).
struct PackedContract {
  uint32_t value{0};

  static PackedContract from(const PdContract &contract) {
    return {(static_cast<uint32_t>(contract.max_v) << 16) | contract.max_i};
  }
  bool is_set() const { return this->value != 0; }
  float voltage() const { return (this->value >> 16) * 0.05f; }
  float current() const { return (this->value & 0xFFFF) * 0.01f; }
};

struct PdMsg {
  uint8_t type{0};
  PdSpecRevision spec_rev{PD_SPEC_REV_2};
  uint8_t id{0};
  uint8_t num_of_obj{0};
  bool extended{false};
  uint32_t data_objects[PD_MAX_NUM_DATA_OBJECTS]{};

  void set_header(uint16_t header);
  uint16_t get_coded_header() const;
};

/// USB-PD sink protocol logic, independent of the PHY chip.
/// Everything except the getters and set_request_voltage() runs in the PHY task, which must also call
/// check_transition_timeout_() and check_request_retry_() regularly; the protocol has no timers of its own.
class PowerDelivery {
 public:
  void set_request_voltage(uint8_t voltage) {
    this->request_voltage_ = std::clamp(voltage, MIN_REQUEST_VOLTAGE, MAX_REQUEST_VOLTAGE);
  }
  uint8_t get_request_voltage() const { return this->request_voltage_; }

  PdState get_state() const { return this->state_; }
  bool is_connected() const { return is_connected_state(this->state_); }

 protected:
  /// Returns false when the message could not be handed to the chip; the PHY logs the failure.
  virtual bool send_message(const PdMsg &msg) = 0;
  /// Called after state_ or contract_ changed, to let the main loop publish it.
  virtual void on_state_changed() = 0;
  /// Called after the source soft reset the protocol; it sends new source capabilities next.
  virtual void on_soft_reset_received() = 0;
  /// Called when source capabilities arrive, before the request for them is sent.
  virtual void on_source_caps_received() = 0;
  /// Called after an accepted request was not followed by PS_RDY in time and the state fell back to default power;
  /// the PHY sends a soft reset so the source offers its capabilities again.
  virtual void on_transition_timeout() = 0;
  /// Called when a request ended without a new contract: it was rejected, the source kept answering Wait or did not
  /// answer in time, or it could not be sent. A contract already in place stays valid.
  virtual void on_request_failed() = 0;

  void handle_message_(const PdMsg &msg);
  /// Called by the PHY when the chip gave up on a message after its retries without receiving a GoodCRC.
  void handle_transmit_failed_();
  void reset_protocol_();
  void set_state_(PdState state);
  void set_contract_(const PdContract &contract);
  void clear_contract_();
  /// Stores both before calling on_state_changed() once, so the main loop never sees the new state with the old
  /// contract.
  void set_state_and_contract_(PdState state, const PdContract &contract);
  void set_ams_(bool active);
  /// Returns true while an atomic message sequence is in progress. Ends it after a timeout, which fails the
  /// request unless the source already accepted a new contract.
  bool check_ams_();
  /// Falls back to default power when the source accepted a request but did not report the new supply ready in time.
  void check_transition_timeout_();
  /// Sends the last request again once tSinkRequest has passed after the source answered Wait.
  void check_request_retry_();

  PdMsg make_control_msg_(PdControlMsgType type) const;
  PdMsg make_data_msg_(PdDataMsgType type, const uint32_t *objects, uint8_t len) const;

  // Shared with the main loop
  std::atomic<PdState> state_{PdState::PD_STATE_DISCONNECTED};
  std::atomic<uint32_t> contract_{0};
  std::atomic<uint8_t> request_voltage_{5};

  // PHY task only
  uint32_t ams_start_{0};
  uint32_t wait_received_ms_{0};
  bool retry_request_pending_{false};

 private:
  void handle_data_message_(const PdMsg &msg);
  void handle_control_message_(const PdMsg &msg);
  void handle_soft_reset_(const PdMsg &msg);
  void respond_to_source_caps_(const PdMsg &msg);
  bool send_request_();
  void request_failed_();

  PdContract requested_contract_{};
  PdContract accepted_contract_{};
  uint32_t last_rdo_{0};
  bool active_ams_{false};
  // Set by Accept; PS_RDY is only valid after the source accepted the request of the current sequence
  bool ps_rdy_expected_{false};
  uint8_t last_received_msg_id_{255};
  uint8_t msg_counter_{0};
  uint8_t wait_retries_{0};
};

}  // namespace esphome::fusb302b
