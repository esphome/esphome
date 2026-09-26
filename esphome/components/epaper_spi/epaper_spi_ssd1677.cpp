#include "epaper_spi_ssd1677.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.ssd1677";

// Both planes are rewritten in full every update (~96 KB at 800x480). Sending that in
// MAX_TRANSFER_TIME slices costs several loop iterations; larger blocks keep the overhead small
// while staying under the scheduler's 50 ms warning.
static constexpr uint32_t TRANSFER_BLOCK_TIME = 40;

void EPaperSSD1677::setup() {
  EPaperMono::setup();
  if (this->is_failed() || !this->is_using_partial_update_())
    return;
  if (!this->sent_.init(this->buffer_length_)) {
    ESP_LOGW(TAG, "No memory for the comparison frame; partial updates will degrade unchanged areas");
  }
}

// Nothing a partial update needs is kept in controller RAM any more, so skip the reset for those.
bool EPaperSSD1677::reset() {
  if (this->update_count_ != 0 && this->sent_.is_valid())
    return true;
  return EPaperMono::reset();
}

bool HOT EPaperSSD1677::transfer_data() {
  if (!this->sent_.is_valid())
    return EPaperMono::transfer_data();

  const auto start_time = millis();
  // A full update ignores 0x26, and the copy may not hold a real frame yet (first update after
  // boot): send the new frame to both planes.
  const bool full = this->update_count_ == 0;
  const size_t row_length = this->row_width_;
  if (this->current_data_index_ == 0) {
    if (this->plane_ == 0) {
      this->x_low_ = 0;
      this->x_high_ = this->width_;
      this->y_low_ = 0;
      this->y_high_ = this->height_;
    }
    this->set_window();
    this->command(this->plane_ == 0 ? 0x26 : 0x24);
  }
  FixedVector<uint8_t> row{};
  row.init(row_length);
  this->start_data_();
  while (this->current_data_index_ != this->height_) {
    size_t data_idx = this->current_data_index_ * row_length;
    if (this->plane_ == 0 && !full) {
      for (size_t i = 0; i != row_length; i++)
        row[i] = this->sent_[data_idx++];
    } else {
      for (size_t i = 0; i != row_length; i++, data_idx++) {
        row[i] = this->buffer_[data_idx];
        if (this->plane_ == 1)
          this->sent_[data_idx] = row[i];
      }
    }
    ++this->current_data_index_;
    this->write_array(&row.front(), row_length);  // NOLINT
    if (millis() - start_time > TRANSFER_BLOCK_TIME) {
      // Let the main loop run and come back next loop
      this->disable();
      return false;
    }
  }
  this->disable();
  this->current_data_index_ = 0;
  if (this->plane_ == 0) {
    this->plane_ = 1;
    return false;
  }
  this->plane_ = 0;
  return true;
}

}  // namespace esphome::epaper_spi
