#include "epaper_uc8179_bwr.h"

#include <algorithm>

#include "colorconv.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.uc8179_bwr";

bool EPaperUC8179BWR::initialise(bool partial) {
  EPaperBase::initialise(partial);  // send the model init sequence
  ESP_LOGV(TAG, "Power on");
  // Power on before the data transfer; the state machine busy-waits for it before TRANSFER_DATA
  this->command(0x04);
  // Give the busy line time to assert before the state machine polls it
  this->next_delay_ = 100;
  return true;
}

void HOT EPaperUC8179BWR::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  const uint32_t pos = (x / 8) + (y * this->row_width_);
  const uint8_t bit = 0x80 >> (x & 0x07);
  const uint32_t red_offset = this->buffer_length_ / 2u;

  auto bwr =
      color_to_bwr<BwrColor>(color, BwrColor::BWR_COLOR_BLACK, BwrColor::BWR_COLOR_WHITE, BwrColor::BWR_COLOR_RED);

  // Update black/white plane (first half of buffer)
  // 0 = black, 1 = white
  if (bwr == BwrColor::BWR_COLOR_WHITE) {
    this->buffer_[pos] |= bit;
  } else {
    this->buffer_[pos] &= ~bit;
  }

  // Update red plane (second half of buffer)
  // 1 = red, or 0 = red when invert_red_ is set (as panels with DDX=11 need)
  if ((bwr == BwrColor::BWR_COLOR_RED) != this->invert_red_) {
    this->buffer_[red_offset + pos] |= bit;
  } else {
    this->buffer_[red_offset + pos] &= ~bit;
  }
}

void EPaperUC8179BWR::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);
    return;
  }

  const size_t half_buffer = this->buffer_length_ / 2u;
  auto bwr =
      color_to_bwr<BwrColor>(color, BwrColor::BWR_COLOR_BLACK, BwrColor::BWR_COLOR_WHITE, BwrColor::BWR_COLOR_RED);

  // B/W plane: 0 = black, 1 = white. Red plane: see draw_pixel_at().
  uint8_t bw_byte = 0xFF;
  bool red = false;
  if (bwr == BwrColor::BWR_COLOR_BLACK) {
    bw_byte = 0x00;
  } else if (bwr == BwrColor::BWR_COLOR_RED) {
    bw_byte = 0x00;
    red = true;
  }
  const uint8_t red_byte = red != this->invert_red_ ? 0xFF : 0x00;
  for (size_t i = 0; i < half_buffer; i++)
    this->buffer_[i] = bw_byte;
  for (size_t i = 0; i < half_buffer; i++)
    this->buffer_[half_buffer + i] = red_byte;

  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
  this->x_low_ = 0;
  this->y_low_ = 0;
}

bool HOT EPaperUC8179BWR::transfer_data() {
  const uint32_t start_time = millis();
  const size_t buffer_length = this->buffer_length_;
  const size_t half_buffer = buffer_length / 2u;

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];

  // The B/W plane (first half) is sent with 0x10, then the red plane (second half) with 0x13
  while (this->current_data_index_ < buffer_length) {
    if (this->current_data_index_ == 0) {
      ESP_LOGV(TAG, "Sending B/W data (0x10)");
      this->command(0x10);
    } else if (this->current_data_index_ == half_buffer) {
      ESP_LOGV(TAG, "Sending Red data (0x13)");
      this->command(0x13);
    }
    const size_t plane_end = this->current_data_index_ < half_buffer ? half_buffer : buffer_length;

    this->start_data_();
    while (this->current_data_index_ < plane_end) {
      const size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, plane_end - this->current_data_index_);
      for (size_t i = 0; i < bytes_to_copy; i++) {
        bytes_to_send[i] = this->buffer_[this->current_data_index_ + i];
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

void EPaperUC8179BWR::power_on() {
  // Power-on is sent at the end of initialise() instead, so that it comes before the data transfer
}

void EPaperUC8179BWR::refresh_screen(bool /* partial */) {
  ESP_LOGV(TAG, "Refresh");
  this->command(0x12);
  this->next_delay_ = 100;
}

void EPaperUC8179BWR::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->command(0x02);
}

void EPaperUC8179BWR::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x07, {0xA5});
}

}  // namespace esphome::epaper_spi
