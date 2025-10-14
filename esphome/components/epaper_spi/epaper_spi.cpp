#include "epaper_spi.h"
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi";

static const LogString *epaper_state_to_string(EPaperState state) {
  switch (state) {
    case EPaperState::IDLE:
      return LOG_STR("IDLE");
    case EPaperState::UPDATE:
      return LOG_STR("UPDATE");
    case EPaperState::RESET:
      return LOG_STR("RESET");
    case EPaperState::INITIALISE:
      return LOG_STR("INITIALISE");
    case EPaperState::TRANSFER_DATA:
      return LOG_STR("TRANSFER_DATA");
    case EPaperState::POWER_ON:
      return LOG_STR("POWER_ON");
    case EPaperState::REFRESH_SCREEN:
      return LOG_STR("REFRESH_SCREEN");
    case EPaperState::POWER_OFF:
      return LOG_STR("POWER_OFF");
    case EPaperState::DEEP_SLEEP:
      return LOG_STR("DEEP_SLEEP");
    default:
      return LOG_STR("UNKNOWN");
  }
}

void EPaperBase::setup() {
  if (!this->init_buffer_(this->get_buffer_length())) {
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

void EPaperBase::setup_pins_() {
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
void EPaperBase::cmd_data(const uint8_t *data) {
  const uint8_t command = data[0];
  const uint8_t length = data[1];
  const uint8_t *ptr = data + 2;

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

bool EPaperBase::is_idle_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }
  return !this->busy_pin_->digital_read();
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
    ESP_LOGE(TAG, "Display update already in progress - %s",
             LOG_STR_ARG(epaper_state_to_string(this->state_queue_.front())));
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

  auto state = this->state_queue_.front();

  switch (state) {
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
  const auto &sequence = this->init_sequence_;
  const size_t sequence_size = this->init_sequence_length_;
  while (index != sequence_size) {
    if (sequence_size - index < 2) {
      this->mark_failed("Malformed init sequence");
      return;
    }
    const auto *ptr = sequence + index;
    const uint8_t length = ptr[1];
    if (sequence_size - index < length + 2) {
      this->mark_failed("Malformed init sequence");
      return;
    }

    this->cmd_data(ptr);
    index += length + 2;
  }

  this->power_on();
}

}  // namespace esphome::epaper_spi
