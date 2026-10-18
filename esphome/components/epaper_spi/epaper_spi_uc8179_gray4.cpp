#include "epaper_spi_uc8179_gray4.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.uc8179_gray4");

bool EPaperUC8179Gray4::initialise(bool partial) {
  EPaperBase::initialise(partial);  // panel configuration and LUTs from the model
  ESP_LOGV(TAG, "Power on");
  this->command(CMD_POWER_ON);
  // Give the busy line time to assert before the state machine polls it
  this->next_delay_ = 100;
  return true;
}

bool HOT EPaperUC8179Gray4::transfer_data() {
  const uint32_t start_time = millis();
  const size_t plane_length = this->buffer_length_ / 2;
  const size_t total = plane_length * 2;
  if (this->current_data_index_ == 0) {
    // Sent after power-on, as on the hardware this was tested with: data polarity and border, then
    // the temperature value, and partial mode off
    this->cmd_data(CMD_CDI, {0xA9, 0x07});
    this->cmd_data(CMD_TSSET, {0x5A});
    this->command(CMD_PARTIAL_OUT);
  }
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  while (this->current_data_index_ < total) {
    const bool high_pass = this->current_data_index_ < plane_length;
    const size_t pass_end = high_pass ? plane_length : total;
    if (this->current_data_index_ == 0 || this->current_data_index_ == plane_length) {
      this->command(high_pass ? CMD_DTM1 : CMD_DTM2);
    }
    this->start_data_();
    while (this->current_data_index_ < pass_end) {
      const size_t count = std::min(MAX_TRANSFER_SIZE, pass_end - this->current_data_index_);
      const size_t src = 2 * (this->current_data_index_ - (high_pass ? 0 : plane_length));
      for (size_t i = 0; i != count; i++)
        bytes_to_send[i] = gray4_plane_byte(this->buffer_[src + 2 * i], this->buffer_[src + 2 * i + 1], high_pass);
      this->write_array(bytes_to_send, count);
      this->current_data_index_ += count;
      if (millis() - start_time > MAX_TRANSFER_TIME) {
        this->disable();
        return false;
      }
    }
    this->disable();
  }
  this->current_data_index_ = 0;
  return true;
}

void EPaperUC8179Gray4::power_on() {
  // Sent at the end of initialise(), since the refresh registers and the data must follow it
}

void EPaperUC8179Gray4::refresh_screen(bool /*partial*/) {
  ESP_LOGV(TAG, "Refresh");
  this->command(CMD_REFRESH);
  // The busy line takes a short time to assert after the refresh command
  this->next_delay_ = 100;
}

void EPaperUC8179Gray4::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->command(CMD_POWER_OFF);
}

}  // namespace esphome::epaper_spi
