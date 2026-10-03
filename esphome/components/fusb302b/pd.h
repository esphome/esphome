#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace esphome::fusb302b {

static constexpr uint8_t PD_MAX_NUM_DATA_OBJECTS = 7;

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
/// Everything except the getters and set_request_voltage() runs in the PHY task.
class PowerDelivery {
 public:
  void set_request_voltage(uint8_t voltage) { this->request_voltage_ = voltage; }
  uint8_t get_request_voltage() const { return this->request_voltage_; }

  PdState get_state() const { return this->state_; }
  bool is_connected() const { return is_connected_state(this->state_); }

 protected:
  virtual bool send_message(const PdMsg &msg) = 0;
  /// Called after state_ or contract_ changed, to let the main loop publish it.
  virtual void on_state_changed() = 0;

  void handle_message_(const PdMsg &msg);
  void reset_protocol_();
  void set_state_(PdState state);
  void set_contract_(const PdContract &contract);
  void clear_contract_();
  void set_ams_(bool active);
  /// Returns true while an atomic message sequence is in progress; ends it after a timeout.
  bool check_ams_();
  /// Returns true when the source accepted a request but did not report the new supply ready in time.
  bool transition_timed_out_();

  PdMsg make_control_msg_(PdControlMsgType type) const;
  PdMsg make_data_msg_(PdDataMsgType type, const uint32_t *objects, uint8_t len) const;

  // Shared with the main loop
  std::atomic<PdState> state_{PdState::PD_STATE_DISCONNECTED};
  std::atomic<uint32_t> contract_{0};
  std::atomic<uint8_t> request_voltage_{5};

  // PHY task only
  bool waiting_for_source_caps_{false};

 private:
  void handle_data_message_(const PdMsg &msg);
  void handle_control_message_(const PdMsg &msg);
  void respond_to_source_caps_(const PdMsg &msg);

  PdContract requested_contract_{};
  PdContract accepted_contract_{};
  uint32_t ams_start_{0};
  bool active_ams_{false};
  uint8_t last_received_msg_id_{255};
  uint8_t msg_counter_{0};
};

}  // namespace esphome::fusb302b
