#include "epaper_spi_ssd1677.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.ssd1677";
static constexpr size_t MAX_TRANSFER_SIZE = 128;

static uint8_t color_to_bit(Color color) {
  // It's always a shade of gray. Map to BLACK or WHITE.
  // We split the luminance at the halfway point (382 = (255*3)/2)
  if ((static_cast<int>(color.r) + color.g + color.b) > 512) {
    return 1;
  }
  return 0;
}

void EPaperSSD1677::power_on() {}

void EPaperSSD1677::power_off() {}

void EPaperSSD1677::refresh_screen() {
  ESP_LOGD(TAG, "Refresh screen");
  this->command(0x22);
  this->data(0xF7);
  this->command(0x20);
}

void EPaperSSD1677::deep_sleep() {
  ESP_LOGD(TAG, "Deep sleep");
  this->command(0x10);
}

bool EPaperSSD1677::reset() {
  if (EPaperBase::reset()) {
    this->command(0x12);
    delay(10);
    return true;
  }
  return false;
}

void EPaperSSD1677::fill(Color color) {
  auto pixel_color = color_to_bit(color) ? 0xFF : 0x00;

  // We store 2 pixels per byte
  this->buffer_.fill(pixel_color);
}

void EPaperSSD1677::clear() {
  // clear buffer to white, just like real paper.
  this->fill(COLOR_ON);
}

void HOT EPaperSSD1677::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->width_ || y >= this->height_ || x < 0 || y < 0)
    return;

  size_t pixel_position = x + y * this->get_width_controller();
  size_t byte_position = pixel_position / 8;
  uint8_t bit_position = pixel_position % 8;
  uint8_t pixel_bit = 0x80 >> bit_position;
  auto original = this->buffer_[byte_position];
  if ((color_to_bit(color) == 0)) {
    this->buffer_[byte_position] = original & ~pixel_bit;
  } else {
    this->buffer_[byte_position] = original | pixel_bit;
  }
}

bool HOT EPaperSSD1677::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  const size_t buffer_length = this->buffer_length_;
  if (this->current_data_index_ == 0) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    this->transfer_start_time_ = millis();
#endif
    ESP_LOGV(TAG, "Start sending data at %ums", (unsigned) millis());
    uint8_t data[4]{};
    if (this->transform_ & (uint8_t) Transform::MIRROR_X) {
      data[0] = this->width_ - 1;
      data[1] = (this->width_ - 1) >> 8;
    } else {
      data[2] = this->width_ - 1;
      data[3] = (this->width_ - 1) >> 8;
    }
    this->cmd_data(0x44, data, sizeof(data));
    if (this->transform_ & (uint8_t) Transform::MIRROR_Y) {
      data[0] = this->height_ - 1;
      data[1] = (this->height_ - 1) >> 8;
      data[2] = 0;
      data[3] = 0;
    } else {
      data[0] = 0;
      data[1] = 0;
      data[2] = this->height_ - 1;
      data[3] = (this->height_ - 1) >> 8;
    }
    this->cmd_data(0x45, data, sizeof(data));
    this->command(0x24);
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
