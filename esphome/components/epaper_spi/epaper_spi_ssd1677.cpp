#include "epaper_spi_ssd1677.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi.ssd1677";

bool EPaperSSD1677::reset() {
  if (this->reset_pin_ == nullptr) {
    return true;
  }

  // Keep reset handling non-blocking within the existing FSM.
  // First call comes in RESET state, second in RESET_END state.
  if (this->state_ == EPaperState::RESET) {
    this->reset_pin_->digital_write(true);
    this->next_delay_ = 20;
    return false;
  }

  this->reset_pin_->digital_write(false);
  delay(2);
  this->reset_pin_->digital_write(true);
  this->next_delay_ = 20;
  return true;
}

bool EPaperSSD1677::initialise(bool partial) {
  if (!EPaperBase::initialise(partial)) {
    return false;
  }

  // Software reset after the model init sequence.
  this->command(0x12);
  this->wait_for_idle_(true);
  return true;
}

void EPaperSSD1677::set_window_() {
  const uint16_t x_start = 0;
  const uint16_t x_end = this->width_ - 1;
  const uint16_t y_start = 0;
  const uint16_t y_end = this->height_ - 1;

  this->cmd_data(
      0x44, {(uint8_t) (x_start & 0xFF), (uint8_t) (x_start >> 8), (uint8_t) (x_end & 0xFF), (uint8_t) (x_end >> 8)});

  this->cmd_data(
      0x45, {(uint8_t) (y_end & 0xFF), (uint8_t) (y_end >> 8), (uint8_t) (y_start & 0xFF), (uint8_t) (y_start >> 8)});

  this->cmd_data(0x4E, {(uint8_t) (x_start & 0xFF), (uint8_t) (x_start >> 8)});
  this->cmd_data(0x4F, {(uint8_t) (y_start & 0xFF), (uint8_t) (y_start >> 8)});
}

bool EPaperSSD1677::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  const size_t buffer_length = this->buffer_length_;

  if (this->current_data_index_ == 0) {
    this->set_window_();
    this->command(0x24);
  }

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  size_t buf_idx = 0;

  this->start_data_();

  while (this->current_data_index_ < buffer_length) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];

    if (buf_idx == sizeof(bytes_to_send)) {
      this->write_array(bytes_to_send, buf_idx);
      buf_idx = 0;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        this->disable();
        return false;
      }
    }
  }

  if (buf_idx != 0) {
    this->write_array(bytes_to_send, buf_idx);
  }

  this->disable();
  this->current_data_index_ = 0;
  return true;
}

void EPaperSSD1677::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen");
  this->cmd_data(0x22, {partial ? (uint8_t) 0xFF : (uint8_t) 0xF7});
  this->command(0x20);
}

void EPaperSSD1677::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x10, {0x01});
}

}  // namespace esphome::epaper_spi
