#include "epaper_spi.h"
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi";

static constexpr const char *const EPAPER_STATE_STRINGS[] = {
    "IDLE", "UPDATE", "RESET", "INITIALISE", "TRANSFER_DATA", "POWER_ON", "REFRESH_SCREEN", "POWER_OFF", "DEEP_SLEEP",
};

static const char *epaper_state_to_string(EPaperState state) {
  if (auto idx = static_cast<unsigned>(state); idx < std::size(EPAPER_STATE_STRINGS))
    return EPAPER_STATE_STRINGS[idx];
  return "Unknown";
}

void EPaperBase::setup() {
  if (!this->init_buffer_(this->buffer_length_)) {
    this->mark_failed("Failed to initialise buffer");
    return;
  }
  this->setup_pins_();
  this->spi_setup();
}

bool EPaperBase::init_buffer_(size_t buffer_length) {
  if (!this->buffer_.init(buffer_length)) {
    return false;
  }
  this->clear();
  return true;
}

void EPaperBase::setup_pins_() const {
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
  }

  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();  // INPUT
  }
}

float EPaperBase::get_setup_priority() const { return setup_priority::PROCESSOR; }

void EPaperBase::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}

void EPaperBase::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

// write a command followed by zero or more bytes of data.
// The command is the first byte, length is the length of data only in the second byte, followed by the data.
// [COMMAND, LENGTH, DATA...]
void EPaperBase::cmd_data(uint8_t command, const uint8_t *ptr, size_t length) {
  ESP_LOGVV(TAG, "Command: 0x%02X, Length: %d, Data: %s", command, length,
            format_hex_pretty(ptr, length, '.', false).c_str());

  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(command);
  if (length > 0) {
    this->dc_pin_->digital_write(true);
    this->write_array(ptr, length);
  }
  this->disable();
}

bool EPaperBase::is_idle_() const {
  if (this->busy_pin_ == nullptr) {
    return true;
  }
  return this->busy_pin_->digital_read();
}

void EPaperBase::reset() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    this->disable_loop();
    this->set_timeout(this->reset_duration_, [this] {
      this->reset_pin_->digital_write(true);
      this->set_timeout(20, [this] { this->enable_loop(); });
    });
  }
}

void EPaperBase::update() {
  if (!this->state_queue_.empty()) {
    ESP_LOGE(TAG, "Display update already in progress - %s", epaper_state_to_string(this->state_queue_.front()));
    return;
  }

  this->state_queue_.push(EPaperState::UPDATE);
  this->state_queue_.push(EPaperState::RESET);
  this->state_queue_.push(EPaperState::INITIALISE);
  this->state_queue_.push(EPaperState::TRANSFER_DATA);
  this->state_queue_.push(EPaperState::POWER_ON);
  this->state_queue_.push(EPaperState::REFRESH_SCREEN);
  this->state_queue_.push(EPaperState::POWER_OFF);
  this->state_queue_.push(EPaperState::DEEP_SLEEP);
  this->state_queue_.push(EPaperState::IDLE);

  this->enable_loop();
}

void EPaperBase::loop() {
  if (this->waiting_for_idle_) {
    if (this->is_idle_()) {
      this->waiting_for_idle_ = false;
    } else {
      if (App.get_loop_component_start_time() - this->waiting_for_idle_last_print_ >= 1000) {
        ESP_LOGV(TAG, "Waiting for idle");
        this->waiting_for_idle_last_print_ = App.get_loop_component_start_time();
      }
      return;
    }
  }

  switch (this->state_queue_.front()) {
    case EPaperState::IDLE:
      this->disable_loop();
      break;
    case EPaperState::UPDATE:
      this->do_update_();  // Calls ESPHome (current page) lambda
      break;
    case EPaperState::RESET:
      this->reset();
      break;
    case EPaperState::INITIALISE:
      this->initialise_();
      break;
    case EPaperState::TRANSFER_DATA:
      if (!this->transfer_data()) {
        return;  // Not done yet, come back next loop
      }
      break;
    case EPaperState::POWER_ON:
      this->power_on();
      break;
    case EPaperState::REFRESH_SCREEN:
      this->refresh_screen();
      break;
    case EPaperState::POWER_OFF:
      this->power_off();
      break;
    case EPaperState::DEEP_SLEEP:
      this->deep_sleep();
      break;
  }
  ESP_LOGV(TAG, "Exiting state %s", epaper_state_to_string(this->state_queue_.front()));
  this->state_queue_.pop();
}

void EPaperBase::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}

void EPaperBase::end_command_() { this->disable(); }

void EPaperBase::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void EPaperBase::end_data_() { this->disable(); }

void EPaperBase::on_safe_shutdown() { this->deep_sleep(); }

void EPaperBase::initialise_() {
  size_t index = 0;

  auto *sequence = this->init_sequence_.data();
  auto length = this->init_sequence_.size();
  while (index != length) {
    if (length - index < 2) {
      this->mark_failed("Malformed init sequence");
      return;
    }
    const uint8_t cmd = sequence[index++];
    if (const uint8_t x = sequence[index++]; x == DELAY_FLAG) {
      ESP_LOGD(TAG, "Delay %dms", cmd);
      delay(cmd);
    } else {
      const uint8_t num_args = x & 0x7F;
      if (length - index < num_args) {
        ESP_LOGE(TAG, "Malformed init sequence, cmd = %X, num_args = %u", cmd, num_args);
        this->mark_failed();
        return;
      }
      const auto *ptr = sequence + index;
      ESP_LOGD(TAG, "Command %02X, length %d", cmd, num_args);
      this->cmd_data(cmd, ptr, num_args);
      index += num_args;
    }
  }
  this->power_on();
}

}  // namespace esphome::epaper_spi
