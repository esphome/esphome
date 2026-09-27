#pragma once

#include <utility>

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_HOERMANN_HCP_IDENTITY
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::hoermann_hcp {

// Door state as reported by the Hoermann bus controller.
enum class DoorState : uint8_t {
  OPEN,
  OPENING,
  CLOSED,
  CLOSING,
  HALF_OPEN,
  MOVE_VENTING,
  VENT,
  MOVE_HALF,
  STOPPED,
};

#ifdef USE_HOERMANN_HCP_IDENTITY
// Payload registers of each value, two bytes each.
static constexpr size_t SERIAL_FIRST_HALF_REGS = 7;
static constexpr size_t SERIAL_SECOND_HALF_REGS = 6;
static constexpr size_t FIRMWARE_REGS = 6;
// Counter of the transfer the read half of the frame acknowledges. Real counters have the 0x80 half marker
// stripped, so this value never occurs.
static constexpr uint8_t NO_TRANSFER_ANSWER = 0xFF;

// Where the identity exchange stands. The low nibble is the code the motor is asked with, 0 while nothing is
// outstanding. Like Hoermann's own bus accessory, the first status answer stays ordinary. Older motors (index B1
// seen) send the whole serial number in one frame, without the half marker.
enum class IdentityPhase : uint8_t {
  IDENTITY_PHASE_IDLE = 0x00,
  IDENTITY_PHASE_DONE = 0x10,
  IDENTITY_PHASE_SERIAL = 0x05,              // A plain frame is the whole serial number.
  IDENTITY_PHASE_SERIAL_SPLIT = 0x15,        // A half marker was seen, so a plain frame can only be the second half.
  IDENTITY_PHASE_SERIAL_SECOND_HALF = 0x25,  // The first half is in.
  IDENTITY_PHASE_FIRMWARE = 0x06,
};
#endif

// Sent once, in a single status answer, as Hoermann's own bus accessory does.
struct HoermannHcpCommand {
  const char *name;
  uint16_t value;
  uint16_t value_2{0x0000};
};

class HoermannHcp : public PollingComponent, public modbus::ModbusServerDevice {
#ifdef USE_HOERMANN_HCP_IDENTITY
  // The motor is asked for these only when one of them is configured.
  SUB_TEXT_SENSOR(serial_number)
  SUB_TEXT_SENSOR(version)
#endif

 public:
  void update() override;
  void dump_config() override;

  // Registered by child entities to be notified when the door state changes.
  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }

  // Modbus server callbacks. The bus controller pushes commands and polls state with 0x17 (the hub runs the write
  // half first, storing the command register that the read half echoes back) and broadcasts status with 0x10.
  modbus::ResponseStatus on_write_registers(uint16_t start_address, const modbus::RegisterValues &registers) override;
  modbus::ResponseStatus on_read_holding_registers(uint16_t start_address, uint16_t number_of_registers,
                                                   modbus::RegisterValues &registers) override;

  // Positions follow the cover convention: 0.0 is fully closed, 1.0 fully open. These return false when the bus
  // controller cannot be asked right now, so the caller can react.
  bool open_door();
  bool close_door();
  bool impulse_door();
  // The door drives to these intermediate positions on its own, so neither takes a target to be stopped at.
  bool vent_door();
  bool half_open_door();
  bool stop_door();
  bool set_position(float position);
  // False while the door has not reported the lamp.
  bool set_light(bool on);

  DoorState get_door_state() const { return this->door_state_; }
  // False until a broadcast has carried a state the door is known to report. Bus traffic alone makes the
  // connection valid, so get_door_state() would still be its default.
  bool is_door_state_known() const { return this->door_state_seen_; }
  float get_current_position() const { return this->current_position_; }
  bool is_valid() const { return this->valid_; }
  bool is_light_on() const { return this->light_on_; }
  // False until a broadcast has actually carried the lamp register. Bus traffic alone makes the connection
  // valid without saying anything about the lamp, so is_light_on() would still be its default.
  bool is_light_known() const { return this->light_seen_; }
  // The requested state while switching, else the reported one.
  bool is_light_heading_on() const { return this->light_requested_ ? this->light_target_ : this->light_on_; }

 protected:
  void record_response_();
  bool command_door_(const HoermannHcpCommand &command);
  // Returns false when the bus controller has not fetched the previous command yet.
  bool queue_command_(const HoermannHcpCommand &command);
  void drop_command_();
  void clear_light_request_();
  void push_command_registers_(modbus::RegisterValues &registers);
  void on_position_reg_(uint16_t value);
  void on_state_reg_(uint16_t value);
  void on_light_reg_(uint16_t value);
#ifdef USE_HOERMANN_HCP_IDENTITY
  // Puts a due request into a status answer.
  void add_identity_request_(modbus::RegisterValues &registers, uint16_t command);
  void arm_identity_request_(IdentityPhase phase);
  uint8_t identity_request_() const { return static_cast<uint8_t>(this->identity_phase_) & 0x0F; }
  // True when the request is due, counting the attempt. Gives up after the last one.
  bool take_identity_request_();
  // Takes a value the motor hands over as a payload transfer. Returns the counter to acknowledge it with, kept or
  // not, or NO_TRANSFER_ANSWER when the frame was something else.
  uint8_t take_identity_transfer_(const modbus::RegisterValues &registers);
  // Acknowledges the transfer taken by the write half of the same frame.
  void push_transfer_answer_(modbus::RegisterValues &registers, uint16_t number_of_registers);
  // Runs from update(), outside the bus callbacks.
  void publish_identity_();
#endif

  void set_valid_(bool valid);
  void set_door_state_(DoorState state);
  // Recomputes the reported position from position_raw_ and the current door state.
  void update_current_position_();
  bool has_target_() const { return this->target_position_ != 0.0f; }
  void clear_target_();
  void set_light_on_(bool on);
  void set_light_seen_(bool seen);

  CallbackManager<void()> state_callback_;

  float current_position_{0.0f};
  // Position the door was told to travel to; 0.0 means no target is armed.
  float target_position_{0.0f};

  const HoermannHcpCommand *next_command_{nullptr};
  uint32_t command_queued_at_{0};
  // Separate from command_queued_at_ so an unrelated command cannot extend the target's start deadline.
  uint32_t target_queued_at_{0};
  uint32_t last_response_{0};
  // Start of the wait for the fetch, then for the report.
  uint32_t light_since_{0};
  uint32_t last_stop_at_{0};
  bool stop_sent_{false};

  // Drop the "connected" flag if the bus controller has not polled us for this long.
  uint16_t connection_timeout_ms_{2000};
  // The state starts on a value the bus controller never reports, so the first broadcast is decoded even when
  // it reads 0x0000.
  uint16_t prev_state_reg_{0xFFFF};
  // 0x17 write half: command register last written to COMMAND_REG. The read half echoes its high-byte message
  // counter and low-byte command back from STATE_REG.
  uint16_t command_reg_value_{0};

  DoorState door_state_{DoorState::CLOSED};
  // Direction the door was started in for the current target. A target armed while the door is still travelling
  // the other way must not be judged by the reported direction until the door has turned around.
  DoorState target_direction_{DoorState::STOPPED};
  // Position as reported by the bus controller, 0..200 across the full travel.
  uint8_t position_raw_{0};
  bool target_started_{false};
  bool valid_{false};
  bool changed_{false};
  bool light_on_{false};
  bool light_seen_{false};
  bool light_requested_{false};
  bool light_command_sent_{false};
  bool light_target_{false};
  bool door_state_seen_{false};
  bool short_broadcast_logged_{false};

#ifdef USE_HOERMANN_HCP_IDENTITY
  uint32_t identity_asked_at_{0};
  IdentityPhase identity_phase_{IdentityPhase::IDENTITY_PHASE_IDLE};
  uint8_t identity_attempts_{0};
  // The request given up on, for update() to report.
  uint8_t identity_unanswered_{0};
  uint8_t transfer_answer_counter_{NO_TRANSFER_ANSWER};
  // Length of an unreadable firmware version left in firmware_version_.
  uint8_t firmware_unreadable_len_{0};
  // A serial number arrived without text, for update() to log.
  bool serial_unreadable_{false};
  bool firmware_unreadable_{false};
  char serial_number_[2 * (SERIAL_FIRST_HALF_REGS + SERIAL_SECOND_HALF_REGS) + 1]{};
  char firmware_version_[2 * FIRMWARE_REGS + 1]{};
#endif
};

}  // namespace esphome::hoermann_hcp
