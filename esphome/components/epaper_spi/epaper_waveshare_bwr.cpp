#include "epaper_waveshare_bwr.h"

#include <algorithm>

namespace esphome::epaper_spi {

enum class BwrState : uint8_t {
  BWR_BLACK,
  BWR_WHITE,
  BWR_RED,
};

static BwrState color_to_bwr(Color color) {
  if (color.r > color.g + color.b && color.r > 127) {
    return BwrState::BWR_RED;
  }
  if (color.r + color.g + color.b >= 382) {
    return BwrState::BWR_WHITE;
  }
  return BwrState::BWR_BLACK;
}

// UC8179 3-color display buffer layout:
// - 1 bit per pixel, 8 pixels per byte
// - Buffer first half: Black/White plane (1=black, 0=white)
// - Buffer second half: Red plane (1=red, 0=white)
// - Total: row_width * height * 2 bytes

void EPaperWaveshareBWR::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  const uint32_t pos = (x / 8) + (y * this->row_width_);
  const uint8_t bit = 0x80 >> (x & 0x07);
  const uint32_t red_offset = this->buffer_length_ / 2u;

  const auto bwr = color_to_bwr(color);

  if (bwr == BwrState::BWR_BLACK) {
    this->buffer_[pos] |= bit;
  } else {
    this->buffer_[pos] &= ~bit;
  }

  if (bwr == BwrState::BWR_RED) {
    this->buffer_[red_offset + pos] |= bit;
  } else {
    this->buffer_[red_offset + pos] &= ~bit;
  }
}

void EPaperWaveshareBWR::fill(Color color) {
  const size_t half_buffer = this->buffer_length_ / 2u;
  const auto bwr = color_to_bwr(color);

  if (bwr == BwrState::BWR_BLACK) {
    // Black plane: 0xFF (black), Red plane: 0x00 (no red)
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[i] = 0xFF;
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[half_buffer + i] = 0x00;
  } else if (bwr == BwrState::BWR_RED) {
    // Black plane: 0x00 (no black), Red plane: 0xFF (red)
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[i] = 0x00;
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[half_buffer + i] = 0xFF;
  } else {
    // Black plane: 0x00 (no black), Red plane: 0x00 (no red)
    this->buffer_.fill(0x00);
  }
}

bool HOT EPaperWaveshareBWR::transfer_data() {
  const uint32_t start_time = millis();
  const size_t buffer_length = this->buffer_length_;
  const size_t half_buffer = buffer_length / 2u;

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];

  // Phase 1: send Black/White plane (first half) via command 0x10 (DTM1)
  // UC8179 DTM1 (0x10): inverted to get 0=black, 1=white
  if (this->current_data_index_ < half_buffer) {
    if (this->current_data_index_ == 0) {
      this->command(0x10);  // DATA START TRANSMISSION 1 (black channel)
    }
    this->start_data_();
    while (this->current_data_index_ < half_buffer) {
      const size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, half_buffer - this->current_data_index_);
      for (size_t i = 0; i < bytes_to_copy; i++) {
        bytes_to_send[i] = ~this->buffer_[this->current_data_index_ + i];
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

  // Phase 2: send Red plane (second half) via command 0x13 (DTM2)
  // UC8179 DTM2 (0x13): 1=red, 0=white
  if (this->current_data_index_ < buffer_length) {
    if (this->current_data_index_ == half_buffer) {
      this->command(0x13);  // DATA START TRANSMISSION 2 (red channel)
    }
    this->start_data_();
    while (this->current_data_index_ < buffer_length) {
      const size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, buffer_length - this->current_data_index_);
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

void EPaperWaveshareBWR::power_on() {
  this->cmd_data(0x01, {0x07, 0x17, 0x3F, 0x3F});  // POWER SETTING
  this->command(0x04);                             // POWER ON
}

void EPaperWaveshareBWR::refresh_screen(bool /*partial*/) {
  this->command(0x12);  // DISPLAY REFRESH
}

void EPaperWaveshareBWR::power_off() {
  this->command(0x02);  // POWER OFF
}

void EPaperWaveshareBWR::deep_sleep() {
  this->cmd_data(0x07, {0xA5});  // DEEP SLEEP with check code
}

}  // namespace esphome::epaper_spi
