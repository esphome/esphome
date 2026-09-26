#pragma once

#include <utility>

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_HOERMANN_HCP_TEXT_SENSOR
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

#ifdef USE_HOERMANN_HCP_TEXT_SENSOR
// Payload registers of each value, two bytes each.
static constexpr size_t SERIAL_FIRST_HALF_REGS = 7;
static constexpr size_t SERIAL_SECOND_HALF_REGS = 6;
static constexpr size_t FIRMWARE_REGS = 6;
#endif

// A HCP command is a simulated key press: the pressed value is presented to the bus controller, then after a
// short delay the released value. Each half also carries a second register, which names the buttons that do
// not fit into the first.
struct HoermannHcpCommand {
  const char *name;
  uint16_t pressed_value;
  uint16_t released_value;
  uint16_t pressed_value_2{0x0000};
  uint16_t released_value_2{0x0000};
  // A door command supersedes a half-open target; the lamp has no bearing on where the door is going.
  bool clears_target{true};
};

class HoermannHcp : public PollingComponent, public modbus::ModbusServerDevice {
#ifdef USE_HOERMANN_HCP_TEXT_SENSOR
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
  bool toggle_light();

  DoorState get_door_state() const { return this->door_state_; }
  float get_current_position() const { return this->current_position_; }
  bool is_valid() const { return this->valid_; }
  bool is_light_on() const { return this->light_on_; }
  // False until a broadcast has actually carried the lamp register. Bus traffic alone makes the connection
  // valid without saying anything about the lamp, so is_light_on() would still be its default.
  bool is_light_known() const { return this->light_seen_; }
  // Where the lamp ends up once every toggle on its way has landed, each of which inverts it. Until then the
  // lamp still reads as its old self, so this is what a request has to be judged against.
  bool is_light_heading_on() const { return this->light_on_ != (this->light_toggles_in_flight_ % 2 != 0); }
  // Drops a lamp toggle the controller has not started reading, so a reversing request cancels it outright
  // instead of fighting it. Returns false if there is nothing to cancel.
  bool cancel_light_toggle();

 protected:
  // True while a lamp toggle is queued but not yet fetched, so the lamp is about to invert.
  bool is_light_toggle_pending_() const;
  // Toggles the door has not been shown yet, which is at most the one still waiting in the command slot.
  uint8_t unsent_light_toggles_() const;
  void record_response_();
  // Returns false when the bus controller has not fetched the previous command yet.
  bool queue_command_(const HoermannHcpCommand &command);
  // Throws away the pending command, taking any armed target with it unless the command was the lamp toggle.
  void drop_command_();
  // One outstanding toggle reached the lamp, was withdrawn, or was thrown away.
  void light_toggle_settled_();
  // Stops expecting the toggles the door has already been shown to reach the lamp.
  void forget_light_toggles_();
  // Appends the two key-press registers and advances the pending command's press/release state.
  void push_command_registers_(modbus::RegisterValues &registers);
  void on_position_reg_(uint16_t value);
  void on_state_reg_(uint16_t value);
  void on_light_reg_(uint16_t value);
#ifdef USE_HOERMANN_HCP_TEXT_SENSOR
  // Puts a due request into a status answer.
  void add_identity_request_(modbus::RegisterValues &registers, uint16_t command);
  void arm_identity_request_(uint8_t request);
  // True when the request is due, counting the attempt. Gives up after the last one.
  bool take_identity_request_(uint32_t now);
  // Takes a value the motor hands over as a payload transfer.
  void take_identity_transfer_(const modbus::RegisterValues &registers);
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

  // Pending command / key-press state machine.
  const HoermannHcpCommand *next_command_{nullptr};
  uint32_t command_queued_at_{0};
  // Separate from command_queued_at_ so an unrelated command cannot extend the target's start deadline.
  uint32_t target_queued_at_{0};
  uint32_t command_written_at_{0};
  uint32_t last_response_{0};
  // When the door was last handed a lamp key press. It reports the lamp a moment later, so this bounds the
  // wait. Queueing another toggle deliberately leaves it alone, so the one already sent keeps its deadline.
  uint32_t light_toggle_released_at_{0};

  // A command is "pressed" for this long before its end value is sent.
  uint16_t key_press_delay_ms_{100};
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
  uint8_t light_toggles_in_flight_{0};
  bool target_started_{false};
  bool valid_{false};
  bool changed_{false};
  bool light_on_{false};
  bool light_seen_{false};
  bool short_broadcast_logged_{false};

#ifdef USE_HOERMANN_HCP_TEXT_SENSOR
  uint32_t identity_asked_at_{0};
  // What the motor is being asked for, 0 while nothing is outstanding.
  uint8_t identity_request_{0};
  uint8_t identity_attempts_{0};
  // The request given up on, for update() to report.
  uint8_t identity_unanswered_{0};
  uint8_t transfer_answer_counter_{0};
  // Length of an unreadable firmware version left in firmware_version_.
  uint8_t firmware_unreadable_len_{0};
  bool identity_started_{false};
  bool serial_first_half_seen_{false};
  // A frame with the half marker arrived, so the serial number comes in two halves.
  bool serial_split_{false};
  bool transfer_answer_pending_{false};
  // A serial number arrived without text, for update() to log.
  bool serial_unreadable_{false};
  bool firmware_unreadable_{false};
  char serial_number_[2 * (SERIAL_FIRST_HALF_REGS + SERIAL_SECOND_HALF_REGS) + 1]{};
  char firmware_version_[2 * FIRMWARE_REGS + 1]{};
#endif
};

}  // namespace esphome::hoermann_hcp
