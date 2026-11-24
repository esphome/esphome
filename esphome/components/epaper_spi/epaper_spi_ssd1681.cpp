#include "epaper_spi_ssd1681.h"

#include <algorithm>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {
ESPHOME_LOG_TAG(TAG, "epaper_spi.ssd1681");

bool HOT EPaperSSD1681::transfer_previous_frame_() {
  const uint32_t start_time = millis();
  if (this->current_data_index_ == 0) {
    this->set_window();
    this->command(0x26);
  }
  this->start_data_();
  const size_t end = this->sent_.size();
  while (this->current_data_index_ != end) {
    size_t length;
    const uint8_t *bytes = this->sent_.get_span(this->current_data_index_, length);
    length = std::min<size_t>({length, end - this->current_data_index_, this->row_width_});
    this->write_array(bytes, length);
    this->current_data_index_ += length;
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      this->disable();
      return false;
    }
  }
  this->disable();
  this->current_data_index_ = 0;
  this->send_red_ = false;
  return false;
}

bool HOT EPaperSSD1681::transfer_data() {
  if (this->current_data_index_ == 0 && this->send_red_) {
    // Both banks always get the whole frame: the RAM ping-pong the panel's OTP enables swaps the bank each
    // command writes after every refresh, so a bank that is only partly written keeps an image from two frames ago
    this->x_low_ = 0;
    this->x_high_ = this->width_;
    this->y_low_ = 0;
    this->y_high_ = this->height_;
    // Without the previous frame there is nothing to compare against, so refresh the whole panel
    if (this->partial_ && !this->sent_valid_)
      this->partial_ = false;
  }
  if (this->partial_ && this->send_red_)
    return this->transfer_previous_frame_();
  return EPaperSSD1683::transfer_data();
}

void EPaperSSD1681::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh screen");
  this->cmd_data(0x3C, {partial ? (uint8_t) 0x80 : (uint8_t) 0x01});
  // 0x26 is compared as it is on a partial refresh and ignored on a full one
  this->cmd_data(0x21, {partial ? (uint8_t) 0x00 : (uint8_t) 0x40, (uint8_t) 0x00});
  // Both end with analog and OSC off before the panel goes to sleep
  this->cmd_data(0x22, {partial ? (uint8_t) 0xFF : (uint8_t) 0xF7});
  this->command(0x20);
}

}  // namespace esphome::epaper_spi
