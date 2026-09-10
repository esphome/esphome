#include "hoermann_hcp.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::hoermann_hcp {

static const char *const TAG = "hoermann_hcp";

// Hoermann HCP holding-register blocks.
static constexpr uint16_t COMMAND_REG = 0x9C41;    // Commands written by the bus controller
static constexpr uint16_t STATE_REG = 0x9CB9;      // Internal state read back by the bus controller
static constexpr uint16_t BROADCAST_REG = 0x9D31;  // Door status broadcast by the bus controller
static constexpr float CLOSE_POSITION_THRESHOLD = 0.05f;
static constexpr float OPEN_POSITION_THRESHOLD = 0.95f;
// Only the parity of the outstanding toggles says where the lamp is heading, so the count must not run away.
static constexpr uint8_t MAX_LIGHT_TOGGLES_IN_FLIGHT = 4;

// Command encoding: the high byte of the first register is the phase (0x02 pressed, 0x01 released) and the
// rest names the button - the low byte for the door commands, the second register for those that do not fit
// there. Both halves repeat that name, so neither register is a level to hold; they carry one event each.
static constexpr HoermannHcpCommand COMMAND_OPEN{"open", 0x0210, 0x0110};
static constexpr HoermannHcpCommand COMMAND_CLOSE{"close", 0x0220, 0x0120};
static constexpr HoermannHcpCommand COMMAND_IMPULSE{"impulse", 0x0240, 0x0140};
// The intermediate positions are named in the second register, so the first only carries the phase.
static constexpr HoermannHcpCommand COMMAND_VENT{"vent", 0x0200, 0x0100, 0x4000, 0x4000};
static constexpr HoermannHcpCommand COMMAND_HALF_OPEN{"half open", 0x0200, 0x0100, 0x0400, 0x0400};
// The lamp is named in the second register, but its phase bytes follow no scheme the door commands share.
static constexpr HoermannHcpCommand COMMAND_TOGGLE_LAMP{"toggle light", 0x0100, 0x0800, 0x0200, 0x0200, false};

// High byte of the state register and the door state it stands for. State 0x00 is decoded separately because
// its low byte tells a plain stop from the vent position.
struct DoorStateMapping {
  uint8_t code;
  DoorState state;
};
static constexpr DoorStateMapping DOOR_STATE_MAPPINGS[] = {
    {0x01, DoorState::OPENING},      {0x02, DoorState::CLOSING},   {0x05, DoorState::MOVE_HALF},
    {0x09, DoorState::MOVE_VENTING}, {0x0A, DoorState::VENT},      {0x20, DoorState::OPEN},
    {0x40, DoorState::CLOSED},       {0x80, DoorState::HALF_OPEN},
};

// The hub rejects a reply whose register count does not match the request, so an unrecognized block length
// is padded with zeros rather than answered with an exception that would fail the controller's whole poll.
static void push_zeros(modbus::RegisterValues &registers, uint16_t count) {
  for (uint16_t i = 0; i < count; i++)
    registers.push_back(0x0000);
}

// True while the door is travelling. An impulse toggles the door, so it only stops one that is moving.
static bool is_moving(DoorState state) {
  switch (state) {
    case DoorState::OPENING:
    case DoorState::CLOSING:
    case DoorState::MOVE_HALF:
    case DoorState::MOVE_VENTING:
      return true;
    default:
      return false;
  }
}

void HoermannHcp::update() {
  const uint32_t now = millis();
  // Time out the connection flag if the bus controller stopped polling.
  if (this->valid_ && now - this->last_response_ > this->connection_timeout_ms_)
    this->set_valid_(false);
  // Status broadcasts alone keep the connection alive, so a command the controller never fetches would
  // otherwise block every later one for as long as it keeps broadcasting.
  if (this->next_command_ != nullptr && now - this->command_queued_at_ > this->connection_timeout_ms_) {
    // Dropping after the press was presented leaves the door without its release value, which is worth saying
    // apart from a command the controller never looked at.
    if (this->command_written_at_ != 0) {
      ESP_LOGW(TAG, "Bus controller stopped polling during '%s' command, dropping it mid key press",
               this->next_command_->name);
    } else {
      ESP_LOGW(TAG, "Bus controller did not fetch '%s' command, dropping it", this->next_command_->name);
    }
    this->drop_command_();
    // Children may have assumed the command would land, so let them re-derive from the door.
    this->changed_ = true;
  }
  // A target waits for a door still travelling the other way to turn around. If it never does, the target has
  // to go as well, otherwise it would cut a later move short. The connection timeout doubles as that window.
  if (this->has_target_() && !this->target_started_ && now - this->target_queued_at_ > this->connection_timeout_ms_) {
    ESP_LOGW(TAG, "Door did not start moving towards the requested position, dropping it");
    this->clear_target_();
  }
  // The door took the lamp key press but never reported the lamp changing, so stop expecting it to.
  if (this->light_toggle_released_at_ != 0 && now - this->light_toggle_released_at_ > this->connection_timeout_ms_) {
    ESP_LOGW(TAG, "Door did not report the lamp changing, giving up on the toggle");
    this->forget_light_toggles_();
  }
  if (this->changed_) {
    this->changed_ = false;
    this->state_callback_.call();
  }
}

void HoermannHcp::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Hoermann HCP bridge:\n"
                "  Modbus server address: 0x%02X",
                this->get_address());
}

modbus::ResponseStatus HoermannHcp::on_read_holding_registers(uint16_t start_address, uint16_t number_of_registers,
                                                              modbus::RegisterValues &registers) {
  if (start_address != STATE_REG) {
    ESP_LOGW(TAG, "Unknown read address 0x%04X", start_address);
    return modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS;
  }

  this->record_response_();

  // 0x17 read half: STATE_REG is read back right after COMMAND_REG was written, so echo the stored message
  // counter (high byte) and command (low byte). The read length identifies which internal block is requested.
  const uint16_t counter = this->command_reg_value_ & 0xFF00;
  const uint16_t command = static_cast<uint16_t>((this->command_reg_value_ & 0x00FF) << 8);

  switch (number_of_registers) {
    case 8:
      // Command request: return the internal state, injecting any pending command.
      registers.push_back(counter);
      registers.push_back(static_cast<uint16_t>(0x0001 | command));
      this->push_command_registers_(registers);
      push_zeros(registers, 4);
      break;
    case 2:
      // Empty command request.
      registers.push_back(static_cast<uint16_t>(0x0004 | counter));
      registers.push_back(command);
      break;
    case 5:
      // Bus scan (the bus controller discovering us, typically at startup).
      ESP_LOGD(TAG, "Bus scan received from bus controller");
      registers.push_back(counter);
      registers.push_back(static_cast<uint16_t>(0x0005 | command));
      registers.push_back(0x0430);
      registers.push_back(0x10FF);
      registers.push_back(0xA845);
      break;
    default:
      ESP_LOGW(TAG, "Unknown read request (read %u registers)", number_of_registers);
      push_zeros(registers, number_of_registers);
      break;
  }

  return {};
}

modbus::ResponseStatus HoermannHcp::on_write_registers(uint16_t start_address,
                                                       const modbus::RegisterValues &registers) {
  if (start_address == COMMAND_REG) {
    // 0x17 write half: stash the command register so the following read half can echo its message counter and
    // command byte back from STATE_REG. The hub always runs the write before the read within one request.
    this->record_response_();
    this->command_reg_value_ = registers[0];
    return {};
  }

  if (start_address != BROADCAST_REG) {
    // Every device sees every broadcast, so a frame meant for another node is ordinary traffic
    ESP_LOGV(TAG, "Ignoring write to address 0x%04X", start_address);
    return modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS;
  }

  this->record_response_();

  // Door status broadcast. The state is decoded first so that a frame reporting both a new state and a new
  // position checks the target against the new state.
  if (registers.size() > 2)
    this->on_state_reg_(registers[2]);
  if (registers.size() > 1)
    this->on_position_reg_(registers[1]);
  if (registers.size() > 6) {
    this->on_light_reg_(registers[6]);
    return {};
  }
  // Nothing refreshes the lamp any more, so what was read before must not be commanded against.
  this->set_light_seen_(false);
  if (!this->short_broadcast_logged_) {
    this->short_broadcast_logged_ = true;
    ESP_LOGD(TAG, "Broadcast of %u registers carries no lamp state", static_cast<unsigned>(registers.size()));
  }
  return {};
}

void HoermannHcp::push_command_registers_(modbus::RegisterValues &registers) {
  const HoermannHcpCommand *command = this->next_command_;
  if (command == nullptr) {
    push_zeros(registers, 2);
    return;
  }
  if (this->command_written_at_ == 0) {
    // First read after the command was queued: present the "key pressed" values.
    this->command_written_at_ = millis();
    ESP_LOGI(TAG, "Sending '%s' command to door", command->name);
    registers.push_back(command->pressed_value);
    registers.push_back(command->pressed_value_2);
    return;
  }
  if (millis() - this->command_written_at_ <= this->key_press_delay_ms_) {
    // Between the two events there is nothing to report, including in the second register.
    push_zeros(registers, 2);
    return;
  }
  // Enough time passed: present the "key released" values and clear the command.
  ESP_LOGD(TAG, "Released '%s' command", command->name);
  this->command_written_at_ = 0;
  this->next_command_ = nullptr;
  // A toggle whose count was already settled, by a lamp change reported from the door's side, has nothing left
  // to wait for, so it must not re-arm the watchdog.
  if (command == &COMMAND_TOGGLE_LAMP && this->light_toggles_in_flight_ != 0)
    this->light_toggle_released_at_ = millis();
  registers.push_back(command->released_value);
  registers.push_back(command->released_value_2);
}

void HoermannHcp::on_position_reg_(uint16_t value) {
  // Low byte: current position.
  const uint8_t position = static_cast<uint8_t>(value);
  if (this->position_raw_ == position)
    return;

  this->position_raw_ = position;
  this->update_current_position_();
  // Until the door actually travels the way it was told to, its position says nothing about the target.
  if (!this->has_target_() || !this->target_started_)
    return;

  // The door only knows "open" and "close", so a half-open target is reached by stopping it on the way.
  const bool reached = this->target_direction_ == DoorState::OPENING
                           ? this->current_position_ >= this->target_position_
                           : this->current_position_ <= this->target_position_;
  if (reached)
    this->stop_door();
}

void HoermannHcp::on_state_reg_(uint16_t value) {
  // The low byte is part of the state for 0x00, so the whole register has to be compared, not just the high byte.
  const uint16_t previous = this->prev_state_reg_;
  this->prev_state_reg_ = value;
  if (previous == value)
    return;

  const uint8_t state = value >> 8;
  if (state == 0x00) {
    // Low byte 0x61 marks the door resting in the vent position, anything else a plain stop.
    this->set_door_state_((value & 0x00FF) == 0x61 ? DoorState::VENT : DoorState::STOPPED);
    return;
  }
  for (const auto &mapping : DOOR_STATE_MAPPINGS) {
    if (mapping.code == state) {
      this->set_door_state_(mapping.state);
      return;
    }
  }
  // The low byte can change on its own, so only report a state we cannot decode once.
  if (state != (previous >> 8))
    ESP_LOGW(TAG, "Unknown door state 0x%02X", state);
}

// Low byte of register 6: bit 0x10 is the lamp, bit 0x04 the relay. The reference implementation records
// 0x00, 0x04, 0x10 and 0x14, so only the lamp bit decides here.
void HoermannHcp::on_light_reg_(uint16_t value) {
  this->set_light_seen_(true);
  this->set_light_on_((value & 0x0010) != 0);
}

bool HoermannHcp::queue_command_(const HoermannHcpCommand &command) {
  if (!this->valid_) {
    // Queueing now would fire the command whenever the controller comes back, which may be much later.
    ESP_LOGW(TAG, "Not connected to the bus controller, dropping '%s' command", command.name);
    return false;
  }
  if (this->next_command_ != nullptr) {
    ESP_LOGW(TAG, "Previous command not yet fetched by the bus controller");
    return false;
  }
  // A new command supersedes any half-open target the door was still travelling to.
  if (command.clears_target)
    this->clear_target_();
  this->next_command_ = &command;
  this->command_queued_at_ = millis();
  return true;
}

bool HoermannHcp::open_door() { return this->queue_command_(COMMAND_OPEN); }
bool HoermannHcp::close_door() { return this->queue_command_(COMMAND_CLOSE); }
bool HoermannHcp::impulse_door() { return this->queue_command_(COMMAND_IMPULSE); }
bool HoermannHcp::vent_door() { return this->queue_command_(COMMAND_VENT); }
bool HoermannHcp::half_open_door() { return this->queue_command_(COMMAND_HALF_OPEN); }
bool HoermannHcp::toggle_light() {
  if (this->light_toggles_in_flight_ >= MAX_LIGHT_TOGGLES_IN_FLIGHT) {
    ESP_LOGW(TAG, "Too many lamp toggles are still waiting to be confirmed, dropping this one");
    return false;
  }
  if (!this->queue_command_(COMMAND_TOGGLE_LAMP))
    return false;
  this->light_toggles_in_flight_++;
  return true;
}
bool HoermannHcp::is_light_toggle_pending_() const { return this->next_command_ == &COMMAND_TOGGLE_LAMP; }

uint8_t HoermannHcp::unsent_light_toggles_() const {
  return this->is_light_toggle_pending_() && this->command_written_at_ == 0 ? 1 : 0;
}

bool HoermannHcp::cancel_light_toggle() {
  // Once the pressed value has been presented the key press is already on the wire, so only an untouched
  // command can be withdrawn.
  if (!this->is_light_toggle_pending_() || this->command_written_at_ != 0)
    return false;
  ESP_LOGD(TAG, "Cancelling '%s' command the controller had not fetched", this->next_command_->name);
  this->drop_command_();
  return true;
}

bool HoermannHcp::stop_door() {
  if (!is_moving(this->door_state_)) {
    this->clear_target_();
    return true;
  }
  // On success queue_command_() clears the target; on refusal it stays armed so the next position retries.
  return this->queue_command_(COMMAND_IMPULSE);
}

bool HoermannHcp::set_position(float position) {
  // The first and last movement segments are inconsistent on some doors, so snap to fully open/closed.
  if (position <= CLOSE_POSITION_THRESHOLD)
    return this->close_door();
  if (position >= OPEN_POSITION_THRESHOLD)
    return this->open_door();
  // Asking the door to travel to where it already is means stopping it.
  if (position == this->current_position_)
    return this->stop_door();

  // The door itself has no notion of a target, so it is started in the right direction and stopped on the way.
  const bool opening = position > this->current_position_;
  if (!this->queue_command_(opening ? COMMAND_OPEN : COMMAND_CLOSE))
    return false;
  this->target_position_ = position;
  this->target_queued_at_ = millis();
  this->target_direction_ = opening ? DoorState::OPENING : DoorState::CLOSING;
  // A door already travelling that way is on its way; one moving the other way has to turn around first.
  this->target_started_ = this->door_state_ == this->target_direction_;
  return true;
}

void HoermannHcp::record_response_() {
  this->last_response_ = millis();
  this->set_valid_(true);
}

void HoermannHcp::set_valid_(bool valid) {
  if (this->valid_ == valid)
    return;
  this->valid_ = valid;
  this->changed_ = true;
  if (valid) {
    ESP_LOGI(TAG, "Bus controller connected");
    return;
  }
  ESP_LOGW(TAG, "Bus controller connection lost (no request for %" PRIu32 "ms)", millis() - this->last_response_);
  // Drop what the controller never fetched, so it neither blocks later commands nor fires on reconnect.
  this->drop_command_();
  // The door cannot be watched while the bus is quiet, so a target left armed would stop it long afterwards.
  this->clear_target_();
  this->forget_light_toggles_();
  // The lamp can be switched at the door while the bus is quiet, so what was last read is no longer trusted.
  this->set_light_seen_(false);
  this->short_broadcast_logged_ = false;
}

void HoermannHcp::drop_command_() {
  const bool was_light_toggle = this->is_light_toggle_pending_();
  // Cleared first so the settling below no longer counts this command among the toggles still to be sent.
  this->next_command_ = nullptr;
  this->command_written_at_ = 0;
  if (was_light_toggle) {
    // A lamp toggle says nothing about where the door was going, so it leaves the target alone.
    this->light_toggle_settled_();
  } else {
    this->clear_target_();
  }
}

void HoermannHcp::light_toggle_settled_() {
  if (this->light_toggles_in_flight_ == 0)
    return;
  this->light_toggles_in_flight_--;
  // Only a toggle the door has been shown can still be confirmed, so unsent ones leave nothing to wait for.
  if (this->light_toggles_in_flight_ == this->unsent_light_toggles_())
    this->light_toggle_released_at_ = 0;
  // The light was showing where the lamp was heading, so it has to be told to look again.
  this->changed_ = true;
}

void HoermannHcp::forget_light_toggles_() {
  // Nothing outstanding must always mean nothing to wait for, or the watchdog below would fire for ever.
  this->light_toggle_released_at_ = 0;
  // A toggle the door has not been shown yet is still going to fire, so it keeps counting.
  const uint8_t unsent = this->unsent_light_toggles_();
  if (this->light_toggles_in_flight_ == unsent)
    return;
  this->light_toggles_in_flight_ = unsent;
  this->changed_ = true;
}

void HoermannHcp::set_door_state_(DoorState state) {
  if (this->door_state_ == state)
    return;
  this->door_state_ = state;
  this->changed_ = true;
  this->update_current_position_();
  if (!this->has_target_())
    return;
  if (state == this->target_direction_) {
    this->target_started_ = true;
  } else if (this->target_started_ && !is_moving(state)) {
    // The door came to rest without reaching the target, so the request it belonged to is over.
    this->clear_target_();
  }
}

void HoermannHcp::update_current_position_() {
  // Doors do not always park at exactly 0 or 200, and Cover::is_fully_closed() is an exact comparison, so
  // trust the reported end stop over the raw count.
  float position = static_cast<float>(this->position_raw_) / 200.0f;
  if (this->door_state_ == DoorState::CLOSED) {
    position = 0.0f;
  } else if (this->door_state_ == DoorState::OPEN) {
    position = 1.0f;
  }
  if (this->current_position_ != position) {
    this->current_position_ = position;
    this->changed_ = true;
  }
}

void HoermannHcp::clear_target_() {
  this->target_position_ = 0.0f;
  this->target_started_ = false;
}

void HoermannHcp::set_light_on_(bool on) {
  if (this->light_on_ == on)
    return;
  this->light_on_ = on;
  this->changed_ = true;
  if (this->light_toggles_in_flight_ <= this->unsent_light_toggles_()) {
    // The door has not been shown a toggle that could explain this, so the lamp was switched at the door.
    ESP_LOGD(TAG, "Lamp %s at the door", ONOFF(on));
    return;
  }
  // The door acted, so one of the toggles it has seen has arrived. Any others still count.
  this->light_toggle_settled_();
}

void HoermannHcp::set_light_seen_(bool seen) {
  if (this->light_seen_ == seen)
    return;
  this->light_seen_ = seen;
  // A resting door changes nothing else, so without this the light would never hear about it.
  this->changed_ = true;
}

}  // namespace esphome::hoermann_hcp
