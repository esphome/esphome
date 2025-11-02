#include "epaper_spi_spectra_e6.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.6c";
static constexpr size_t MAX_TRANSFER_SIZE = 128;

static uint8_t color_to_hex(Color color) {
  if (!color.is_on())
    return 0x0;  // black
  if (color.red > 127) {
    if (color.green > 170) {
      if (color.blue > 127)
        return 0x1;  // White
      return 0x2;    // Yellow
    }
    return 0x3;  // Red (or Magenta)
  }
  if (color.green > 127) {
    if (color.blue > 127)
      return 0x5;  // Cyan -> Blue
    return 0x6;    // Green
  }
  if (color.blue > 127)
    return 0x5;  // Blue
  return 0x0;    // Black
}

void EPaperSpectraE6::power_on() {
  ESP_LOGD(TAG, "Power on");
  this->command(0x04);
}

void EPaperSpectraE6::power_off() {
  ESP_LOGD(TAG, "Power off");
  this->command(0x02);
  this->data(0x00);
}

void EPaperSpectraE6::refresh_screen() {
  ESP_LOGD(TAG, "Refresh");
  this->command(0x12);
  this->data(0x00);
}

void EPaperSpectraE6::deep_sleep() {
  ESP_LOGD(TAG, "Deep sleep");
  this->command(0x07);
  this->data(0xA5);
}

void EPaperSpectraE6::fill(Color color) {
  auto pixel_color = color_to_hex(color);

  // We store 2 pixels per byte
  this->buffer_.fill(pixel_color + (pixel_color << 4));
}

void EPaperSpectraE6::clear() {
  // clear buffer to white, just like real paper.
  this->fill(COLOR_ON);
}

void HOT EPaperSpectraE6::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->width_ || y >= this->height_ || x < 0 || y < 0)
    return;

  auto pixel_bits = color_to_hex(color);
  uint32_t pixel_position = x + y * this->get_width_controller();
  uint32_t byte_position = pixel_position / 2;
  auto original = this->buffer_[byte_position];
  if ((pixel_position & 1) != 0) {
    this->buffer_[byte_position] = (original & 0xF0) | pixel_bits;
  } else {
    this->buffer_[byte_position] = (original & 0x0F) | (pixel_bits << 4);
  }
}

bool HOT EPaperSpectraE6::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  const size_t buffer_length = this->buffer_length_;
  if (this->current_data_index_ == 0) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    this->transfer_start_time_ = millis();
#endif
    ESP_LOGV(TAG, "Start sending data at %ums", (unsigned) millis());
    this->command(0x10);
  }

  size_t buf_idx = 0;
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  while (this->current_data_index_ != buffer_length) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];

    if (buf_idx == sizeof bytes_to_send) {
      this->start_data_();
      this->write_array(bytes_to_send, buf_idx);
      this->end_data_();
      ESP_LOGV(TAG, "Wrote %d bytes at %ums", buf_idx, (unsigned) millis());
      buf_idx = 0;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        // Let the main loop run and come back next loop
        return false;
      }
    }
  }
  // Finished the entire dataset
  if (buf_idx != 0) {
    this->start_data_();
    this->write_array(bytes_to_send, buf_idx);
    this->end_data_();
  }
  this->current_data_index_ = 0;
  ESP_LOGV(TAG, "Sent data in %" PRIu32 " ms", millis() - this->transfer_start_time_);
  return true;
}
}  // namespace esphome::epaper_spi
