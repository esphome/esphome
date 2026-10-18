#include "epaper_spi_uc8179_gray4.h"

#include <algorithm>

namespace esphome::epaper_spi {

// The order the panel was tested with: power on right after the power setting, then the rest of the setup
// and the LUTs while powered. The busy wait between the two steps is armed here, since the state does not change
bool EPaperUC8179Gray4::initialise(bool partial) {
  if (!this->init_powered_) {
    this->cmd_data(CMD_POWER_SETTING, {0x07, 0x07, 0x3F, 0x3F, 0x03});
    this->command(CMD_POWER_ON);
    this->init_powered_ = true;
    this->delay_until_ = millis() + 100;
    this->wait_for_idle_(true);
    return false;
  }
  this->init_powered_ = false;
  return EPaperBase::initialise(partial);
}

bool HOT EPaperUC8179Gray4::transfer_data() {
  const uint32_t start_time = millis();
  const size_t plane_length = this->buffer_length_ / 2;
  if (this->current_data_index_ == 0) {
    // Only accepted after power-on; these replace the CDI from the init sequence
    this->cmd_data(CMD_CDI, {0xA9, 0x07});
    this->cmd_data(CMD_TSSET, {0x5A});
    this->command(CMD_PARTIAL_OUT);
  }
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  while (this->current_data_index_ < this->buffer_length_) {
    const bool high_pass = this->current_data_index_ < plane_length;
    const size_t pass_start = high_pass ? 0 : plane_length;
    const size_t pass_end = pass_start + plane_length;
    if (this->current_data_index_ == pass_start)
      this->command(high_pass ? CMD_DTM1 : CMD_DTM2);
    this->start_data_();
    while (this->current_data_index_ < pass_end) {
      const size_t count = std::min(MAX_TRANSFER_SIZE, pass_end - this->current_data_index_);
      const size_t src = 2 * (this->current_data_index_ - pass_start);
      size_t span_length;
      const uint8_t *span = this->buffer_.get_span(src, span_length);
      if (span_length >= 2 * count) {
        for (size_t i = 0; i != count; i++)
          bytes_to_send[i] = gray4_plane_byte(span[2 * i], span[2 * i + 1], high_pass);
      } else {
        // the chunk straddles two of the split buffer's blocks
        for (size_t i = 0; i != count; i++)
          bytes_to_send[i] = gray4_plane_byte(this->buffer_[src + 2 * i], this->buffer_[src + 2 * i + 1], high_pass);
      }
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

}  // namespace esphome::epaper_spi
