#include "epaper_spi_uc8179_gray4.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

void EPaperUC8179Gray4::setup() {
  EPaperUC8179::setup();
  if (!this->is_failed())
    this->init_sent_frame_(this->plane_length_());
}

void EPaperUC8179Gray4::fill(Color color) {
  if (this->get_clipping().is_set()) {
    // Falls back to the generic per-pixel implementation for the clipped rectangle.
    EPaperBase::fill(color);
    return;
  }
  const uint8_t level = color_to_gray4(color);
  this->buffer_.fill((uint8_t) (level | (level << 2) | (level << 4) | (level << 6)));
  this->mark_all_drawn_();
}

void HOT EPaperUC8179Gray4::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  const uint8_t level = color_to_gray4(color);
  const size_t byte_position = (size_t) y * this->row_width_ + x / 4;
  const uint8_t shift = (uint8_t) (6 - 2 * (x % 4));  // most significant pixel first
  const uint8_t original = this->buffer_[byte_position];
  this->buffer_[byte_position] = (uint8_t) ((original & ~(0x03 << shift)) | (level << shift));
}

// Sent after power-on, like EPaperUC8179's refresh mode, since the waveform registers need it.
void EPaperUC8179Gray4::set_gray4_mode_(bool partial) {
  // Four-level OTP waveform, forced via the temperature registers
  this->cmd_data(CMD_CCSET, {0x02});
  this->cmd_data(CMD_TSSET, {0x5F});
  if (!partial)
    return;
  // LUTs from the registers the init sequence loaded; the polarity and border setting go with them
  this->cmd_data(CMD_PSR, {0x3F});
  this->cmd_data(CMD_CDI, {0xA9, 0x07});
  this->set_partial_window_(0, this->width_ - 1, 0, this->height_ - 1);
}

// A full update sends each level's high bit to DTM1 and its low bit to DTM2. A partial update sends
// the frame on the panel to DTM1 and the new one to DTM2, as high bits inverted. sent_ keeps the
// high bits of each frame sent, for the next partial update.
bool HOT EPaperUC8179Gray4::transfer_data() {
  const uint32_t start_time = millis();
  const bool partial = this->is_partial_push_();
  const size_t plane_length = this->plane_length_();
  if (this->current_data_index_ == 0)
    this->set_gray4_mode_(partial);

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  while (this->current_data_index_ != 2 * plane_length) {
    const bool first_plane = this->current_data_index_ < plane_length;
    const size_t plane_start = first_plane ? 0 : plane_length;
    if (this->current_data_index_ == plane_start)
      this->command(first_plane ? CMD_DTM1 : CMD_DTM2);
    this->start_data_();
    while (this->current_data_index_ != plane_start + plane_length) {
      const size_t index = this->current_data_index_ - plane_start;
      const size_t length = std::min(MAX_TRANSFER_SIZE, plane_length - index);
      if (first_plane && partial) {
        for (size_t i = 0; i != length; i++)
          bytes_to_send[i] = (uint8_t) ~this->sent_[index + i];
      } else {
        // The high bits go out on DTM1 of a full update and DTM2 of a partial one
        const bool high_bit = first_plane != partial;
        for (size_t i = 0; i != length; i++) {
          const size_t src = 2 * (index + i);
          bytes_to_send[i] = gray4_plane_byte(this->buffer_[src], this->buffer_[src + 1], high_bit);
        }
        if (high_bit)
          this->record_sent_(index, bytes_to_send, length);
        if (partial) {
          for (size_t i = 0; i != length; i++)
            bytes_to_send[i] = (uint8_t) ~bytes_to_send[i];
        }
      }
      this->write_array(bytes_to_send, length);
      this->current_data_index_ += length;
      if (this->current_data_index_ != 2 * plane_length && millis() - start_time > MAX_TRANSFER_TIME) {
        // Let the main loop run and come back next loop
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
