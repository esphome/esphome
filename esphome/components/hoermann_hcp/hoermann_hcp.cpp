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

static constexpr HoermannHcpCommand COMMAND_OPEN{"open", 0x0210, 0x0110};
static constexpr HoermannHcpCommand COMMAND_CLOSE{"close", 0x0220, 0x0120};
static constexpr HoermannHcpCommand COMMAND_IMPULSE{"impulse", 0x0240, 0x0140};

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
    ESP_LOGW(TAG, "Bus controller did not fetch '%s' command, dropping it", this->next_command_->name);
    this->next_command_ = nullptr;
    this->command_written_at_ = 0;
    this->clear_target_();
  }
  // A target waits for a door still travelling the other way to turn around. If it never does, the target has
  // to go as well, otherwise it would cut a later move short. The connection timeout doubles as that window.
  if (this->has_target_() && !this->target_started_ && now - this->command_queued_at_ > this->connection_timeout_ms_) {
    ESP_LOGW(TAG, "Door did not start moving towards the requested position, dropping it");
    this->clear_target_();
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
    registers.push_back(0x0000);
    return;
  }
  if (millis() - this->command_written_at_ <= this->key_press_delay_ms_) {
    // Still inside the key-press window, so keep presenting 0x0000.
    push_zeros(registers, 2);
    return;
  }
  // Enough time passed: present the "key released" values and clear the command.
  ESP_LOGD(TAG, "Released '%s' command", command->name);
  this->command_written_at_ = 0;
  this->next_command_ = nullptr;
  registers.push_back(command->released_value);
  registers.push_back(0x0000);
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
  this->clear_target_();
  this->next_command_ = &command;
  this->command_queued_at_ = millis();
  return true;
}

bool HoermannHcp::open_door() { return this->queue_command_(COMMAND_OPEN); }
bool HoermannHcp::close_door() { return this->queue_command_(COMMAND_CLOSE); }
bool HoermannHcp::impulse_door() { return this->queue_command_(COMMAND_IMPULSE); }

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
  this->next_command_ = nullptr;
  this->command_written_at_ = 0;
  this->clear_target_();
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

}  // namespace esphome::hoermann_hcp
