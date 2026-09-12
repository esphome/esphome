#pragma once

#include <utility>

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

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

// Steps of announcing that the accessory is about to go quiet.
enum class PauseState : uint8_t {
  PAUSE_STATE_IDLE,
  PAUSE_STATE_WAITING_FOR_ACK,
  PAUSE_STATE_SETTLING,
  PAUSE_STATE_DONE,
};

// A HCP command is one event: the value is presented to the bus controller once, on the next poll that fetches
// it. The second register names the buttons that do not fit into the first.
struct HoermannHcpCommand {
  const char *name;
  uint16_t value;
  uint16_t value_2{0x0000};
  // A door command supersedes a half-open target; the lamp has no bearing on where the door is going.
  bool clears_target{true};
};

class HoermannHcp : public PollingComponent, public modbus::ModbusServerDevice {
 public:
  void update() override;
  void dump_config() override;
  void on_shutdown() override;

  /** Tells the bus controller the accessory is about to go quiet, then waits briefly for it to answer.
   *
   * The controller registered this accessory during its bus scan, so one that simply stops answering is a
   * fault to it rather than an absence. Blocks, serving the bus itself, because neither caller runs where
   * the main loop would turn it. Goes quiet either way; the return value only says whether it was
   * acknowledged.
   */
  bool announce_pause();

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
  // Asks for the lamp to be on or off. A request still waiting to be fetched is replaced rather than refused.
  bool set_light(bool on);

  DoorState get_door_state() const { return this->door_state_; }
  float get_current_position() const { return this->current_position_; }
  bool is_valid() const { return this->valid_; }
  bool is_light_on() const { return this->light_on_; }
  // False until a broadcast has actually carried the lamp register. Bus traffic alone makes the connection
  // valid without saying anything about the lamp, so is_light_on() would still be its default.
  bool is_light_known() const { return this->light_seen_; }
  // The lamp is reported a moment after it was told, so until then a request is what the lamp is heading for.
  bool is_light_heading_on() const { return this->light_request_pending_ ? this->light_request_on_ : this->light_on_; }

 protected:
  void record_response_();
  // Returns false when the bus controller has not fetched the previous command yet.
  bool queue_command_(const HoermannHcpCommand &command);
  // Throws away the pending command, taking any armed target with it unless the command was a lamp request.
  void drop_command_();
  // Stops expecting the lamp to report the state it was asked for.
  void forget_light_request_();
  // Appends the two command registers, spending the pending command.
  void push_command_registers_(modbus::RegisterValues &registers);
  void on_position_reg_(uint16_t value);
  void on_state_reg_(uint16_t value);
  void on_light_reg_(uint16_t value);

  // Reads a payload transfer and arms the answer the controller expects on the read half of the same request.
  void take_transfer_(const modbus::RegisterValues &registers);
  void push_transfer_answer_(modbus::RegisterValues &registers, uint16_t number_of_registers);
  /** Moves the announcement along one step and reports whether anything is left to say.
   *
   * Decides everything from the clock so it can be called repeatedly, which is what lets the restart path
   * and the ota trigger share the same steps and makes them testable without a bus.
   */
  bool advance_pause_();
  // Drops a command the controller has not been shown yet. Unlike drop_command_ it keeps the travel target:
  // nothing can be sent until the announcement is over, but if no restart follows, the next position the
  // door reports still stops it where it was told to stop.
  void drop_unsent_command_();
  // Ends the announcement, however it ended, and puts the device back to answering normally.
  void end_pause_();
  // The pause replaces the ordinary state answer until the whole announcement is over.
  bool announcing_pause_() const {
    return this->pause_state_ == PauseState::PAUSE_STATE_WAITING_FOR_ACK ||
           this->pause_state_ == PauseState::PAUSE_STATE_SETTLING;
  }

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

  // The command waiting for the controller's next fetch. There is one slot: the controller takes one per poll.
  const HoermannHcpCommand *next_command_{nullptr};
  // The command the controller has just fetched, waiting to be logged off the answer path.
  const HoermannHcpCommand *sent_command_{nullptr};
  uint32_t command_queued_at_{0};
  // Separate from command_queued_at_ so an unrelated command cannot extend the target's start deadline.
  uint32_t target_queued_at_{0};
  uint32_t last_response_{0};
  // When the door fetched the lamp request. It reports the lamp a moment later, so this bounds the wait; zero
  // while the request is still in the slot.
  uint32_t light_request_sent_at_{0};
  uint32_t pause_started_at_{0};

  // Drop the "connected" flag if the bus controller has not polled us for this long.
  uint16_t connection_timeout_ms_{2000};
  // How long the announcement waits to be acknowledged. The controller polls several times a second.
  uint16_t pause_ack_timeout_ms_{800};
  // Nothing arriving for this long means no telegram is in flight, so a restart lands between them.
  uint16_t pause_quiet_ms_{40};
  // A controller that never falls quiet must not hold up the restart for longer than this.
  uint16_t pause_settle_ms_{200};
  // Ceiling on the whole announcement, so a controller that keeps a key press open cannot hold it open.
  uint16_t pause_total_timeout_ms_{1500};
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
  // A lamp request the door has not reported back yet, and which way it asked.
  bool light_request_pending_{false};
  bool light_request_on_{false};
  // Running counter and code of a payload transfer still to be answered on the following read half.
  uint8_t transfer_answer_counter_{0};
  uint8_t transfer_answer_code_{0};
  bool transfer_answer_pending_{false};
  PauseState pause_state_{PauseState::PAUSE_STATE_IDLE};
  bool pause_confirmed_{false};
  // Guards the blocking announcement against being entered from inside itself.
  bool announcing_{false};
  bool target_started_{false};
  bool valid_{false};
  bool changed_{false};
  bool light_on_{false};
  bool light_seen_{false};
  bool short_broadcast_logged_{false};
};

}  // namespace esphome::hoermann_hcp
