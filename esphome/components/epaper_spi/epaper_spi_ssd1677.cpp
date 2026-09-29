#include "epaper_spi_ssd1677.h"

#include <algorithm>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.ssd1677";

void EPaperSSD1677::setup() {
  EPaperMono::setup();
  if (!this->is_failed())
    this->init_comparison_frame_();
}

void EPaperSSD1677::init_comparison_frame_() {
  if (!this->is_using_partial_update_())
    return;
  if (!this->sent_.init(this->plane_row_length_() * this->height_)) {
    ESP_LOGW(TAG, "No memory for the comparison frame; partial updates will degrade unchanged areas");
  }
}

void EPaperSSD1677::plane_row(size_t y, uint8_t *out) {
  const size_t row_length = this->plane_row_length_();
  const size_t data_idx = y * row_length;
  for (size_t i = 0; i != row_length; i++)
    out[i] = this->buffer_[data_idx + i];
}

// Nothing a partial update needs is kept in controller RAM any more, so skip the reset for those.
bool EPaperSSD1677::reset() {
  if (this->update_count_ != 0 && this->sent_.is_valid())
    return true;
  return EPaperMono::reset();
}

// The window always covers the whole panel, so each plane is the frame's bytes in order. Where
// those bytes are already stored as the plane needs them (the comparison frame, and a 1-bit
// buffer) they are written straight from the buffer, as many at a time as the time slice allows;
// otherwise they are built a row at a time by plane_row().
bool HOT EPaperSSD1677::transfer_data() {
  if (!this->sent_.is_valid())
    return EPaperMono::transfer_data();

  const auto start_time = millis();
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
  // A full update ignores 0x26, and the copy may not hold a real frame yet (first update after
  // boot): send the new frame to both planes.
  const bool send_copy = this->plane_ == 0 && this->update_count_ != 0;
  const bool direct = send_copy || this->buffer_is_plane();
  const auto &source = send_copy ? this->sent_ : this->buffer_;
  const size_t row_length = this->plane_row_length_();
  const size_t plane_length = row_length * this->height_;
  // Roughly what the bus moves in one time slice, so a slice is not overrun by much
  const size_t max_chunk = std::max<size_t>(this->data_rate_ / 8000 * MAX_TRANSFER_TIME, MAX_TRANSFER_SIZE);
  SmallBufferWithHeapFallback<128> row_alloc(direct ? 0 : row_length);
  this->start_data_();
  while (this->current_data_index_ != plane_length) {
    size_t length;
    const uint8_t *data;
    if (direct) {
      data = source.get_span(this->current_data_index_, length);
      length = std::min(length, max_chunk);
    } else {
      // Always at the start of a row here, since this path sends whole rows only
      this->plane_row(this->current_data_index_ / row_length, row_alloc.get());
      data = row_alloc.get();
      length = row_length;
    }
    this->write_array(data, length);
    if (this->plane_ == 1)
      this->sent_.write(this->current_data_index_, data, length);
    this->current_data_index_ += length;
    if (this->current_data_index_ != plane_length && millis() - start_time > MAX_TRANSFER_TIME) {
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
