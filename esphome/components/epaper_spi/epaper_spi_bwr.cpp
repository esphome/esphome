#include "epaper_spi_bwr.h"

#include <algorithm>
#include <cstring>

#include "colorconv.h"

namespace esphome::epaper_spi {

namespace {
// Whether a pixel of this color sets its bit in the black/white plane
bool sets_bw_bit(BwrColor bwr, bool red_sets_black) {
  return bwr == BwrColor::BWR_COLOR_BLACK || (bwr == BwrColor::BWR_COLOR_RED && red_sets_black);
}
}  // namespace

void HOT EPaperBWR::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  const uint32_t pos = (x / 8) + (y * this->row_width_);
  const uint8_t bit = 0x80 >> (x & 0x07);
  const uint32_t red_offset = this->buffer_length_ / 2u;

  const auto bwr =
      color_to_bwr<BwrColor>(color, BwrColor::BWR_COLOR_BLACK, BwrColor::BWR_COLOR_WHITE, BwrColor::BWR_COLOR_RED);

  assign_bits(this->buffer_[pos], bit, sets_bw_bit(bwr, this->red_sets_black_));
  assign_bits(this->buffer_[red_offset + pos], bit, bwr == BwrColor::BWR_COLOR_RED);
}

void EPaperBWR::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);
    return;
  }

  const size_t half_buffer = this->buffer_length_ / 2u;
  const auto bwr =
      color_to_bwr<BwrColor>(color, BwrColor::BWR_COLOR_BLACK, BwrColor::BWR_COLOR_WHITE, BwrColor::BWR_COLOR_RED);

  this->fill_range_(0, half_buffer, sets_bw_bit(bwr, this->red_sets_black_) ? 0xFF : 0x00);
  this->fill_range_(half_buffer, this->buffer_length_, bwr == BwrColor::BWR_COLOR_RED ? 0xFF : 0x00);

  this->mark_all_drawn_();
}

void EPaperBWR::fill_range_(size_t index, size_t end, uint8_t value) {
  while (index != end) {
    size_t length;
    uint8_t *data = this->buffer_.get_span(index, length);
    length = std::min(length, end - index);
    memset(data, value, length);
    index += length;
  }
}

bool HOT EPaperBWR::send_buffer_range_(size_t end, uint8_t invert_mask, uint32_t start_time) {
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  this->start_data_();
  while (this->current_data_index_ < end) {
    const size_t chunk = std::min(MAX_TRANSFER_SIZE, end - this->current_data_index_);
    // The chunk may cross from one SplitBuffer part to the next, so copy it a span at a time
    for (size_t filled = 0; filled != chunk;) {
      size_t length;
      const uint8_t *data = this->buffer_.get_span(this->current_data_index_ + filled, length);
      length = std::min(length, chunk - filled);
      for (size_t i = 0; i != length; i++)
        bytes_to_send[filled + i] = data[i] ^ invert_mask;
      filled += length;
    }
    this->write_array(bytes_to_send, chunk);
    this->current_data_index_ += chunk;
    if (this->current_data_index_ < end && millis() - start_time > MAX_TRANSFER_TIME) {
      this->disable();
      return false;  // yield; resume next loop
    }
  }
  this->disable();
  return true;
}

bool HOT EPaperBWR::transfer_data() {
  const uint32_t start_time = millis();
  const size_t half_buffer = this->buffer_length_ / 2u;

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
