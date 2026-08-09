#include "hoermann_hcp.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::hoermann_hcp {

static const char *const TAG = "hoermann_hcp";

// Simulated key-press duration: a command is "pressed" for this long before its end value is sent.
static constexpr uint32_t SIMULATE_KEY_PRESS_DELAY_MS = 100;
// Drop the "connected" flag if the bus controller has not polled us for this long.
static constexpr uint32_t CONNECTION_TIMEOUT_MS = 2000;

// Hoermann HCP holding-register blocks.
static constexpr uint16_t COMMAND_REG = 0x9C41;    // Commands written by the bus controller
static constexpr uint16_t STATE_REG = 0x9CB9;      // Internal state read back by the bus controller
static constexpr uint16_t BROADCAST_REG = 0x9D31;  // Door status broadcast by the bus controller

// Command definitions: {name, reg_plus2_start, reg_plus2_end, reg_plus3_start, reg_plus3_end}.
static constexpr HoermannHcpCommand COMMAND_OPEN{"open", 0x0210, 0x0110, 0x0000, 0x0000};
static constexpr HoermannHcpCommand COMMAND_CLOSE{"close", 0x0220, 0x0120, 0x0000, 0x0000};
static constexpr HoermannHcpCommand COMMAND_IMPULSE{"impulse", 0x0240, 0x0140, 0x0000, 0x0000};
static constexpr HoermannHcpCommand COMMAND_TOGGLE_LAMP{"toggle light", 0x0100, 0x0800, 0x0200, 0x0200};

// The hub rejects a reply whose register count does not match the request, so an unrecognized block length
// is padded with zeros rather than answered with an exception that would fail the controller's whole poll.
static void fill_with_zeros(uint16_t number_of_registers, modbus::RegisterValues &registers) {
  for (uint16_t i = 0; i < number_of_registers; i++)
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
  // Time out the connection flag if the bus controller stopped polling.
  if (this->valid_ && millis() - this->last_response_ > CONNECTION_TIMEOUT_MS) {
    this->set_valid_(false);
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
  this->record_response_();

  if (start_address != STATE_REG) {
    ESP_LOGW(TAG, "Unknown read address 0x%04X", start_address);
    return modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS;
  }

  // 0x17 read half: STATE_REG is read back right after COMMAND_REG was written, so echo the stored message
  // counter (high byte) and command (low byte). The read length identifies which internal block is requested.
  const uint16_t counter = this->command_reg_value_ & 0xFF00;
  const uint16_t command = static_cast<uint16_t>((this->command_reg_value_ & 0x00FF) << 8);

  switch (number_of_registers) {
    case 8: {
      // Command request: return the internal state, injecting any pending command.
      uint16_t reg_plus2;
      uint16_t reg_plus3;
      this->get_command_values_to_read_(reg_plus2, reg_plus3);
      registers.push_back(counter);
      registers.push_back(static_cast<uint16_t>(0x0001 | command));
      registers.push_back(reg_plus2);
      registers.push_back(reg_plus3);
      fill_with_zeros(4, registers);
      break;
    }
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
      fill_with_zeros(number_of_registers, registers);
      break;
  }

  return {};
}

modbus::ResponseStatus HoermannHcp::on_write_registers(uint16_t start_address,
                                                       const modbus::RegisterValues &registers) {
  this->record_response_();

  if (start_address == COMMAND_REG) {
    // 0x17 write half: stash the command register so the following read half can echo its message counter and
    // command byte back from STATE_REG. The hub always runs the write before the read within one request.
    this->command_reg_value_ = registers[0];
    return {};
  }

  if (start_address != BROADCAST_REG) {
    ESP_LOGW(TAG, "Unknown write address 0x%04X", start_address);
    return modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS;
  }

  // Door status broadcast. Each handler compares the previous register value with the new one to detect
  // high/low byte changes, so the previous values are tracked between broadcasts. The state is decoded first
  // so that a frame reporting both a new state and a new position checks the target against the new state.
  if (registers.size() > 2) {
    this->on_current_state_changed_(this->prev_state_reg_, registers[2]);
    this->prev_state_reg_ = registers[2];
  }
  if (registers.size() > 1) {
    this->on_door_position_changed_(this->prev_position_reg_, registers[1]);
    this->prev_position_reg_ = registers[1];
  }
  if (registers.size() > 6) {
    this->on_light_changed_(this->prev_light_reg_, registers[6]);
    this->prev_light_reg_ = registers[6];
  }
  return {};
}

void HoermannHcp::get_command_values_to_read_(uint16_t &reg_plus2, uint16_t &reg_plus3) {
  reg_plus2 = 0x0000;
  reg_plus3 = 0x0000;
  if (this->next_command_ == nullptr)
    return;

  if (this->command_written_at_ == 0) {
    // First read after the command was queued: present the "key pressed" values.
    reg_plus2 = this->next_command_->reg_plus2_start;
    reg_plus3 = this->next_command_->reg_plus3_start;
    this->command_written_at_ = millis();
    ESP_LOGI(TAG, "Sending '%s' command to door", this->next_command_->name);
  } else if (millis() - this->command_written_at_ > SIMULATE_KEY_PRESS_DELAY_MS) {
    // Enough time passed: present the "key released" values and clear the command.
    reg_plus2 = this->next_command_->reg_plus2_end;
    reg_plus3 = this->next_command_->reg_plus3_end;
    ESP_LOGD(TAG, "Released '%s' command", this->next_command_->name);
    this->command_written_at_ = 0;
    this->next_command_ = nullptr;
  }
  // Within the key-press window we keep presenting 0x0000.
}

void HoermannHcp::on_door_position_changed_(uint16_t old_value, uint16_t new_value) {
  // Low byte: current position.
  if ((old_value & 0x00FF) == (new_value & 0x00FF))
    return;

  this->position_raw_ = new_value & 0x00FF;
  this->update_current_position_();
  if (this->goto_position_ == 0.0f)
    return;

  // The door only knows "open" and "close", so a half-open target is reached by stopping it on the way.
  const bool reached = (this->door_state_ == DoorState::CLOSING && this->current_position_ <= this->goto_position_) ||
                       (this->door_state_ == DoorState::OPENING && this->current_position_ >= this->goto_position_);
  if (reached)
    this->stop_door();
}

void HoermannHcp::on_current_state_changed_(uint16_t old_value, uint16_t new_value) {
  // The low byte is part of the state for 0x00, so the whole register has to be compared, not just the high byte.
  if (old_value == new_value)
    return;

  const uint8_t state = new_value >> 8;
  switch (state) {
    case 0x01:
      this->set_door_state_(DoorState::OPENING);
      break;
    case 0x02:
      this->set_door_state_(DoorState::CLOSING);
      break;
    case 0x20:
      this->set_door_state_(DoorState::OPEN);
      break;
    case 0x40:
      this->set_door_state_(DoorState::CLOSED);
      break;
    case 0x80:
      this->set_door_state_(DoorState::HALF_OPEN);
      break;
    case 0x09:
      this->set_door_state_(DoorState::MOVE_VENTING);
      break;
    case 0x05:
      this->set_door_state_(DoorState::MOVE_HALF);
      break;
    case 0x0A:
      this->set_door_state_(DoorState::VENT);
      break;
    case 0x00:
      // Low byte 0x61 marks the door resting in the vent position, anything else a plain stop.
      this->set_door_state_((new_value & 0x00FF) == 0x61 ? DoorState::VENT : DoorState::STOPPED);
      break;
    default:
      // The low byte can change on its own, so only report a state we cannot decode once.
      if (state != (old_value >> 8))
        ESP_LOGW(TAG, "Unknown door state 0x%02X", state);
  }
}

void HoermannHcp::on_light_changed_(uint16_t old_value, uint16_t new_value) {
  // Low byte: light state.
  if ((old_value & 0x00FF) != (new_value & 0x00FF)) {
    uint16_t low = new_value & 0x00FF;
    this->set_light_on_(low == 0x14 || low == 0x10);
  }
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
  this->goto_position_ = 0.0f;
  this->next_command_ = &command;
  return true;
}

void HoermannHcp::open_door() { this->queue_command_(COMMAND_OPEN); }
void HoermannHcp::close_door() { this->queue_command_(COMMAND_CLOSE); }
void HoermannHcp::impulse_door() { this->queue_command_(COMMAND_IMPULSE); }
void HoermannHcp::toggle_light() { this->queue_command_(COMMAND_TOGGLE_LAMP); }

void HoermannHcp::turn_light(bool on) {
  if (on != this->light_on_)
    this->queue_command_(COMMAND_TOGGLE_LAMP);
}

void HoermannHcp::stop_door() {
  if (!is_moving(this->door_state_)) {
    this->goto_position_ = 0.0f;
    return;
  }
  // On success queue_command_() clears the target; on refusal it stays armed so the next position retries.
  this->queue_command_(COMMAND_IMPULSE);
}

void HoermannHcp::set_position(float position) {
  // The first and last movement segments are inconsistent on some doors, so snap to fully open/closed.
  if (position <= 0.05f) {
    this->close_door();
    return;
  }
  if (position >= 0.95f) {
    this->open_door();
    return;
  }
  if (position == this->current_position_) {
    // Asking the door to travel to where it already is means stopping it.
    this->stop_door();
    return;
  }

  // The door itself has no notion of a target, so it is started in the right direction and stopped on the way.
  const bool opening = position > this->current_position_;
  if (this->queue_command_(opening ? COMMAND_OPEN : COMMAND_CLOSE))
    this->goto_position_ = position;
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
  this->goto_position_ = 0.0f;
}

void HoermannHcp::set_door_state_(DoorState state) {
  if (this->door_state_ == state)
    return;
  this->door_state_ = state;
  this->changed_ = true;
  // The door came to rest without reaching the target, so the request it belonged to is over.
  if (!is_moving(state))
    this->goto_position_ = 0.0f;
  this->update_current_position_();
}

void HoermannHcp::update_current_position_() {
  // Doors do not always park at exactly 0 or 200, and Cover::is_fully_closed() is an exact comparison, so
  // trust the reported end stop over the raw count.
  switch (this->door_state_) {
    case DoorState::CLOSED:
      this->set_current_position_(0.0f);
      break;
    case DoorState::OPEN:
      this->set_current_position_(1.0f);
      break;
    default:
      this->set_current_position_(static_cast<float>(this->position_raw_) / 200.0f);
      break;
  }
}

void HoermannHcp::set_current_position_(float position) {
  if (this->current_position_ != position) {
    this->current_position_ = position;
    this->changed_ = true;
  }
}

void HoermannHcp::set_light_on_(bool on) {
  if (this->light_on_ != on) {
    this->light_on_ = on;
    this->changed_ = true;
  }
}

}  // namespace esphome::hoermann_hcp
