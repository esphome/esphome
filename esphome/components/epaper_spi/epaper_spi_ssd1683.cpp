#include "epaper_spi_ssd1683.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.mono";

void EPaperSSD1683::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen");
  this->cmd_data(0x3C, {partial ? (uint8_t) 0x80 : (uint8_t) 0x01});
  // On partial update, set red RAM to inverse to remove BW ghosting
  this->cmd_data(0x21, {partial ? (uint8_t) 0x80 : (uint8_t) 0x40, (uint8_t) 0x00});
  // Set full update to 0xD7 for fast update, 0xF7 for normal
  // Fast update is not actually faster, it takes the same amount of time but flashes less
  // Manufacturer recommends not using fast update all the time, TODO expose this to the user
  this->cmd_data(0x22, {partial ? (uint8_t) 0xFC : (uint8_t) 0xF7});
  this->command(0x20);
}

void EPaperSSD1683::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->command(0x10);
}

bool EPaperSSD1683::reset() {
  if (EPaperBase::reset()) {
    this->command(0x12);
    return true;
  }
  return false;
}

void EPaperSSD1683::set_window() {
  // round x-coordinates to byte boundaries
  this->x_low_ &= ~7;
  this->x_high_ += 7;
  this->x_high_ /= 8;

  this->cmd_data(0x44, {(uint8_t) this->x_low_, (uint8_t) (this->x_high_ - 1)});
  this->cmd_data(0x45, {(uint8_t) this->y_low_, (uint8_t) (this->y_low_ / 256), (uint8_t) (this->y_high_ - 1),
                        (uint8_t) ((this->y_high_ - 1) / 256)});
  this->cmd_data(0x4E, {(uint8_t) this->x_low_});
  this->cmd_data(0x4F, {(uint8_t) this->y_low_, (uint8_t) (this->y_low_ / 256)});
}

bool HOT EPaperSSD1683::transfer_data() {
  auto start_time = millis();
  if (this->current_data_index_ == 0) {
    if (this->send_red_) {
      // round to byte boundaries
      this->set_window();
    }
    // for monochrome, we need to send red on every refresh to prevent dirty pixels
    // when doing a partial refresh
    this->command(this->send_red_ ? 0x26 : 0x24);
    this->current_data_index_ = this->y_low_;  // actually current line
  }
  size_t row_length = this->x_high_ - this->x_low_;
  FixedVector<uint8_t> bytes_to_send{};
  bytes_to_send.init(row_length);
  ESP_LOGV(TAG, "Writing %u bytes at line %zu at %ums", row_length, this->current_data_index_, (unsigned) millis());
  this->start_data_();
  while (this->current_data_index_ != this->y_high_) {
    size_t data_idx = this->current_data_index_ * this->row_width_ + this->x_low_;
    for (size_t i = 0; i != row_length; i++) {
      bytes_to_send[i] = this->buffer_[data_idx++];
    }
    ++this->current_data_index_;
    this->write_array(&bytes_to_send.front(), row_length);  // NOLINT
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      this->disable();
      return false;
    }
  }

  this->disable();
  this->current_data_index_ = 0;
  if (this->send_red_) {
    this->send_red_ = false;
    return false;
  }
  this->send_red_ = true;
  return true;
}

}  // namespace esphome::epaper_spi
