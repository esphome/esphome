#include "epaper_spi_bwr.h"

#include "colorconv.h"

namespace esphome::epaper_spi {

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

bool HOT EPaperBWR::send_buffer_range_(size_t end, uint8_t invert_mask, uint32_t start_time) {
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  size_t buf_idx = 0;
  while (this->current_data_index_ < end) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++] ^ invert_mask;
    if (buf_idx == sizeof bytes_to_send) {
      this->start_data_();
      this->write_array(bytes_to_send, buf_idx);
      this->disable();
      buf_idx = 0;
      if (millis() - start_time > MAX_TRANSFER_TIME)
        return false;  // yield; resume next loop
    }
  }
  if (buf_idx != 0) {
    this->start_data_();
    this->write_array(bytes_to_send, buf_idx);
    this->disable();
  }
  return true;
}

bool HOT EPaperBWR::transfer_data() {
  const uint32_t start_time = millis();
  const size_t half_buffer = this->buffer_length_ / 2;

  // Black/white plane (first half, always inverted) then red plane (second half, inverted if invert_red_).
  if (this->current_data_index_ == 0)
    this->command(0x10);
  if (this->current_data_index_ < half_buffer && !this->send_buffer_range_(half_buffer, 0xFF, start_time))
    return false;

  if (this->current_data_index_ == half_buffer)
    this->command(0x13);
  if (!this->send_buffer_range_(this->buffer_length_, this->invert_red_ ? 0xFF : 0x00, start_time))
    return false;

  this->current_data_index_ = 0;
  return true;
}

}  // namespace esphome::epaper_spi
