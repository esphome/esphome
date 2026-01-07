#include "epaper_spi_ssd1677.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.ssd1677";

void EPaperSSD1677::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen");
  this->command(0x22);
  this->data(partial ? 0xFF : 0xF7);
  this->command(0x20);
}

void EPaperSSD1677::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->command(0x10);
}

bool EPaperSSD1677::reset() {
  if (EPaperBase::reset()) {
    this->command(0x12);
    return true;
  }
  return false;
}

bool HOT EPaperSSD1677::transfer_data() {
  auto start_time = millis();
  if (this->current_data_index_ == 0) {
    uint8_t data[4]{};
    // round to byte boundaries
    this->x_low_ &= ~7;
    this->y_low_ &= ~7;
    this->x_high_ += 7;
    this->x_high_ &= ~7;
    this->y_high_ += 7;
    this->y_high_ &= ~7;
    data[0] = this->x_low_;
    data[1] = this->x_low_ / 256;
    data[2] = this->x_high_ - 1;
    data[3] = (this->x_high_ - 1) / 256;
    cmd_data(0x4E, data, 2);
    cmd_data(0x44, data, sizeof(data));
    data[0] = this->y_low_;
    data[1] = this->y_low_ / 256;
    data[2] = this->y_high_ - 1;
    data[3] = (this->y_high_ - 1) / 256;
    cmd_data(0x4F, data, 2);
    this->cmd_data(0x45, data, sizeof(data));
    // for monochrome, we still need to clear the red data buffer at least once to prevent it
    // causing dirty pixels after partial refresh.
    this->command(this->send_red_ ? 0x26 : 0x24);
    this->current_data_index_ = this->y_low_;  // actually current line
  }
  size_t row_length = (this->x_high_ - this->x_low_) / 8;
  FixedVector<uint8_t> bytes_to_send{};
  bytes_to_send.init(row_length);
  ESP_LOGV(TAG, "Writing bytes at line %zu at %ums", this->current_data_index_, (unsigned) millis());
  this->start_data_();
  while (this->current_data_index_ != this->y_high_) {
    size_t data_idx = (this->current_data_index_ * this->width_ + this->x_low_) / 8;
    for (size_t i = 0; i != row_length; i++) {
      bytes_to_send[i] = this->send_red_ ? 0 : this->buffer_[data_idx++];
    }
    ++this->current_data_index_;
    this->write_array(&bytes_to_send.front(), row_length);  // NOLINT
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      this->end_data_();
      return false;
    }
  }

  this->end_data_();
  this->current_data_index_ = 0;
  if (this->send_red_) {
    this->send_red_ = false;
    return false;
  }
  return true;
}

}  // namespace esphome::epaper_spi
