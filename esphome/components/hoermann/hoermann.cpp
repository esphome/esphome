#include "hoermann.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::hoermann {

static const char *const TAG = "hoermann";

// Simulated key-press duration: a command is "pressed" for this long before its end value is sent.
static constexpr uint32_t SIMULATE_KEY_PRESS_DELAY_MS = 100;
// Drop the "connected" flag if the bus controller has not polled us for this long.
static constexpr uint32_t CONNECTION_TIMEOUT_MS = 2000;

// Hoermann HCP holding-register blocks.
static constexpr uint16_t COMMAND_REG = 0x9C41;    // Commands written by the bus controller
static constexpr uint16_t STATE_REG = 0x9CB9;      // Internal state read back by the bus controller
static constexpr uint16_t BROADCAST_REG = 0x9D31;  // Door status broadcast by the bus controller

// Command definitions: {reg_plus2_start, reg_plus2_end, reg_plus3_start, reg_plus3_end}.
static constexpr HoermannCommand COMMAND_OPEN{0x0210, 0x0110, 0x0000, 0x0000};
static constexpr HoermannCommand COMMAND_CLOSE{0x0220, 0x0120, 0x0000, 0x0000};
static constexpr HoermannCommand COMMAND_IMPULSE{0x0240, 0x0140, 0x0000, 0x0000};

void Hoermann::setup() {
  ESP_LOGCONFIG(TAG, "Waiting for the bus controller to start polling Modbus address 0x%02X", this->get_address());
}

void Hoermann::update() {
  // Time out the connection flag if the bus controller stopped polling.
  if (this->valid_ && millis() - this->last_response_ > CONNECTION_TIMEOUT_MS) {
    this->set_valid_(false);
  }
  if (this->changed_) {
    this->changed_ = false;
    this->state_callback_.call();
  }
}

void Hoermann::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Hoermann HCP bridge:\n"
                "  Modbus server address: 0x%02X\n"
                "  Connection timeout: %ums",
                this->get_address(), static_cast<unsigned>(CONNECTION_TIMEOUT_MS));
}

modbus::ServerResponseStatus Hoermann::on_modbus_read_write_registers(uint16_t read_start_address,
                                                                      uint16_t number_of_registers,
                                                                      uint16_t write_start_address,
                                                                      const modbus::RegisterValues &write_registers,
                                                                      modbus::RegisterValues &read_registers) {
  this->record_response_();

  if (write_start_address == COMMAND_REG && read_start_address == STATE_REG && !write_registers.empty()) {
    // High byte of the first written register is a message counter, low byte is the command.
    uint16_t counter = write_registers[0] & 0xFF00;
    uint16_t command = (write_registers[0] & 0x00FF) << 8;

    if (write_registers.size() == 2 && number_of_registers == 8) {
      // Command request: return the internal state, injecting any pending command.
      uint16_t reg_plus2;
      uint16_t reg_plus3;
      this->get_command_values_to_read_(reg_plus2, reg_plus3);
      read_registers.push_back(static_cast<uint16_t>(0x0000 | counter));
      read_registers.push_back(static_cast<uint16_t>(0x0001 | command));
      read_registers.push_back(reg_plus2);
      read_registers.push_back(reg_plus3);
      read_registers.push_back(0x0000);
      read_registers.push_back(0x0000);
      read_registers.push_back(0x0000);
      read_registers.push_back(0x0000);
    } else if (write_registers.size() == 2 && number_of_registers == 2) {
      // Empty command request.
      read_registers.push_back(static_cast<uint16_t>(0x0004 | counter));
      read_registers.push_back(static_cast<uint16_t>(0x0000 | command));
    } else if (write_registers.size() == 3 && number_of_registers == 5) {
      // Bus scan (the bus controller discovering us, typically at startup).
      ESP_LOGD(TAG, "Bus scan received from bus controller");
      read_registers.push_back(static_cast<uint16_t>(0x0000 | counter));
      read_registers.push_back(static_cast<uint16_t>(0x0005 | command));
      read_registers.push_back(0x0430);
      read_registers.push_back(0x10ff);
      read_registers.push_back(0xa845);
    } else {
      ESP_LOGW(TAG, "Unknown read/write request (write %u, read %u)", static_cast<unsigned>(write_registers.size()),
               number_of_registers);
      for (uint16_t i = 0; i < number_of_registers; i++)
        read_registers.push_back(0x0000);
    }
  } else {
    ESP_LOGW(TAG, "Unknown read/write addresses write=0x%04X read=0x%04X", write_start_address, read_start_address);
    for (uint16_t i = 0; i < number_of_registers; i++)
      read_registers.push_back(0x0000);
  }

  return {};
}

modbus::ServerResponseStatus Hoermann::on_modbus_write_registers(uint16_t start_address,
                                                                 const modbus::RegisterValues &registers) {
  this->record_response_();

  if (start_address != BROADCAST_REG) {
    ESP_LOGW(TAG, "Unknown write address 0x%04X", start_address);
    return {};
  }

  // Door status broadcast. Each handler compares the previous register value with the new one to detect
  // high/low byte changes, so the previous values are tracked between broadcasts.
  if (registers.size() > 1) {
    this->on_door_position_changed_(this->prev_position_reg_, registers[1]);
    this->prev_position_reg_ = registers[1];
  }
  if (registers.size() > 2) {
    this->on_current_state_changed_(this->prev_state_reg_, registers[2]);
    this->prev_state_reg_ = registers[2];
  }
  return {};
}

void Hoermann::get_command_values_to_read_(uint16_t &reg_plus2, uint16_t &reg_plus3) {
  reg_plus2 = 0x0000;
  reg_plus3 = 0x0000;
  if (this->next_command_ == nullptr)
    return;

  if (this->command_written_at_ == 0) {
    // First read after the command was queued: present the "key pressed" values.
    reg_plus2 = this->next_command_->reg_plus2_start;
    reg_plus3 = this->next_command_->reg_plus3_start;
    this->command_written_at_ = millis();
  } else if (millis() - this->command_written_at_ > SIMULATE_KEY_PRESS_DELAY_MS) {
    // Enough time passed: present the "key released" values and clear the command.
    reg_plus2 = this->next_command_->reg_plus2_end;
    reg_plus3 = this->next_command_->reg_plus3_end;
    this->command_written_at_ = 0;
    this->next_command_ = nullptr;
  }
  // Within the key-press window we keep presenting 0x0000.
}

void Hoermann::on_door_position_changed_(uint16_t old_value, uint16_t new_value) {
  // Low byte: current position.
  if ((old_value & 0x00FF) != (new_value & 0x00FF)) {
    this->set_current_position_(static_cast<float>(new_value & 0x00FF) / 200.0f);
    bool reached = (this->goto_position_ > 0.0f && this->door_state_ == DoorState::CLOSING &&
                    this->goto_position_ >= this->current_position_) ||
                   (this->goto_position_ > 0.0f && this->door_state_ == DoorState::OPENING &&
                    this->goto_position_ <= this->current_position_);
    if (reached) {
      this->stop_door();
      this->goto_position_ = 0.0f;
    }
  }
}

void Hoermann::on_current_state_changed_(uint16_t old_value, uint16_t new_value) {
  if ((old_value & 0xFF00) == (new_value & 0xFF00))
    return;

  switch ((new_value & 0xFF00) >> 8) {
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
      this->set_door_state_((new_value & 0x00FF) == 0x61 ? DoorState::VENT : DoorState::STOPPED);
      break;
    default:
      ESP_LOGW(TAG, "Unknown door state 0x%02X", (new_value & 0xFF00) >> 8);
  }
}

void Hoermann::queue_command_(bool condition, const HoermannCommand &command) {
  if (!condition)
    return;
  if (this->next_command_ != nullptr) {
    ESP_LOGW(TAG, "Previous command not yet fetched by the bus controller");
    return;
  }
  this->next_command_ = &command;
}

void Hoermann::open_door() { this->queue_command_(true, COMMAND_OPEN); }
void Hoermann::close_door() { this->queue_command_(true, COMMAND_CLOSE); }
void Hoermann::impulse_door() { this->queue_command_(true, COMMAND_IMPULSE); }

void Hoermann::stop_door() {
  // Only send an impulse if the door is actually moving.
  this->queue_command_(this->door_state_ == DoorState::CLOSING || this->door_state_ == DoorState::OPENING ||
                           this->door_state_ == DoorState::MOVE_HALF || this->door_state_ == DoorState::MOVE_VENTING,
                       COMMAND_IMPULSE);
}

void Hoermann::set_position(int percent) {
  // The first and last movement segments are inconsistent on some doors, so snap to fully open/closed.
  if (percent <= 5) {
    this->close_door();
  } else if (percent >= 95) {
    this->open_door();
  } else {
    this->goto_position_ = static_cast<float>(percent) / 100.0f;
    this->queue_command_(this->current_position_ < this->goto_position_, COMMAND_OPEN);
    this->queue_command_(this->current_position_ > this->goto_position_, COMMAND_CLOSE);
  }
}

void Hoermann::record_response_() {
  this->last_response_ = millis();
  this->set_valid_(true);
}

void Hoermann::set_valid_(bool valid) {
  if (this->valid_ == valid)
    return;
  this->valid_ = valid;
  this->changed_ = true;
  if (valid) {
    ESP_LOGI(TAG, "Bus controller connected");
  } else {
    ESP_LOGW(TAG, "Bus controller connection lost (no request for %ums)",
             static_cast<unsigned>(millis() - this->last_response_));
  }
}
void Hoermann::set_door_state_(DoorState state) {
  if (this->door_state_ != state) {
    this->door_state_ = state;
    this->changed_ = true;
  }
}
void Hoermann::set_current_position_(float position) {
  if (this->current_position_ != position) {
    this->current_position_ = position;
    this->changed_ = true;
  }
}

}  // namespace esphome::hoermann
