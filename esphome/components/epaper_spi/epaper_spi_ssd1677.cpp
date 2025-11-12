#include "epaper_spi_ssd1677.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.ssd1677";

void EPaperSSD1677::refresh_screen(bool partial) {
  static uint8_t x = 0;
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
    data[2] = this->width_ - 1;
    data[3] = (this->width_ - 1) / 256;
    cmd_data(0x4E, data, 2);
    cmd_data(0x44, data, sizeof(data));
    data[2] = this->height_ - 1;
    data[3] = (this->height_ - 1) / 256;
    cmd_data(0x4F, data, 2);
    this->cmd_data(0x45, data, sizeof(data));
    this->command(0x24);
  }
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  size_t buf_idx = 0;
  ESP_LOGV(TAG, "Writing bytes at offset %zu at %ums", this->current_data_index_, (unsigned) millis());
  this->start_data_();
  while (this->current_data_index_ != this->buffer_length_) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];
    if (buf_idx == sizeof(bytes_to_send)) {
      this->write_array(bytes_to_send, buf_idx);
      buf_idx = 0;
      if (millis() - start_time > MAX_TRANSFER_TIME) {
        // Let the main loop run and come back next loop
        this->end_data_();
        return false;
      }
    }
  }
  if (buf_idx != 0) {
    this->write_array(bytes_to_send, buf_idx);
    ESP_LOGV(TAG, "Wrote %d bytes at %ums", buf_idx, (unsigned) millis());
  }
  this->end_data_();
  this->current_data_index_ = 0;
  return true;
}
}  // namespace esphome::epaper_spi
