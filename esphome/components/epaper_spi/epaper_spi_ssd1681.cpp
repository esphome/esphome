#include "epaper_spi_ssd1681.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
ESPHOME_LOG_TAG(TAG, "epaper_spi.ssd1681");

bool HOT EPaperSSD1681::transfer_data() {
  if (this->current_data_index_ == 0 && this->send_red_) {
    if (this->update_count_ == 0 || this->restore_previous_) {
      // A refresh drives the whole panel from RAM, so a full one, or the first after the panel lost its
      // RAM, makes both RAM banks whole again
      this->x_low_ = 0;
      this->x_high_ = this->width_;
      this->y_low_ = 0;
      this->y_high_ = this->height_;
    } else {
      // 0x26 already holds the image on the panel: the controller copies 0x24 into it after each refresh
      this->bounds_from_changes_();
      this->set_window();
      this->send_red_ = false;
    }
  }
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
