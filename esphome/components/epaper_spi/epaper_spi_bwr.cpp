#include "epaper_spi_bwr.h"

#include <algorithm>

#include "colorconv.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.bwr";

void HOT EPaperBWR::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  const uint32_t pos = (x / 8) + (y * this->row_width_);
  const uint8_t bit = 0x80 >> (x & 0x07);
  const uint32_t red_offset = this->buffer_length_ / 2u;

  const auto bwr =
      color_to_bwr<BwrColor>(color, BwrColor::BWR_COLOR_BLACK, BwrColor::BWR_COLOR_WHITE, BwrColor::BWR_COLOR_RED);

  if (bwr == BwrColor::BWR_COLOR_BLACK) {
    this->buffer_[pos] |= bit;
  } else {
    this->buffer_[pos] &= ~bit;
  }

  if (bwr == BwrColor::BWR_COLOR_RED) {
    this->buffer_[red_offset + pos] |= bit;
  } else {
    this->buffer_[red_offset + pos] &= ~bit;
  }
}

void EPaperBWR::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);
    return;
  }

  const size_t half_buffer = this->buffer_length_ / 2u;
  const auto bwr =
      color_to_bwr<BwrColor>(color, BwrColor::BWR_COLOR_BLACK, BwrColor::BWR_COLOR_WHITE, BwrColor::BWR_COLOR_RED);

  const uint8_t bw_byte = bwr == BwrColor::BWR_COLOR_BLACK ? 0xFF : 0x00;
  const uint8_t red_byte = bwr == BwrColor::BWR_COLOR_RED ? 0xFF : 0x00;
  for (size_t i = 0; i < half_buffer; i++)
    this->buffer_[i] = bw_byte;
  for (size_t i = 0; i < half_buffer; i++)
    this->buffer_[half_buffer + i] = red_byte;

  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
  this->x_low_ = 0;
  this->y_low_ = 0;
}

bool HOT EPaperBWR::transfer_data() {
  const uint32_t start_time = millis();
  const size_t buffer_length = this->buffer_length_;
  const size_t half_buffer = buffer_length / 2u;

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];

  // The B/W plane (first half) is sent inverted with 0x10, then the red plane (second half) with 0x13
  while (this->current_data_index_ < buffer_length) {
    if (this->current_data_index_ == 0) {
      ESP_LOGV(TAG, "Sending B/W data (0x10)");
      this->command(0x10);
    } else if (this->current_data_index_ == half_buffer) {
      ESP_LOGV(TAG, "Sending Red data (0x13)");
      this->command(0x13);
    }
    const bool bw_plane = this->current_data_index_ < half_buffer;
    const size_t plane_end = bw_plane ? half_buffer : buffer_length;
    const uint8_t invert_mask = (bw_plane || this->invert_red_) ? 0xFF : 0x00;

    this->start_data_();
    while (this->current_data_index_ < plane_end) {
      const size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, plane_end - this->current_data_index_);
      for (size_t i = 0; i < bytes_to_copy; i++) {
        bytes_to_send[i] = this->buffer_[this->current_data_index_ + i] ^ invert_mask;
      }
      this->write_array(bytes_to_send, bytes_to_copy);
      this->current_data_index_ += bytes_to_copy;

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
