#include "epaper_spi_ssd1677.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.ssd1677";

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
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_ - 1;
  this->y_high_ = this->height_ - 1;
}

void EPaperSSD1677::clear() {
  // clear buffer to white, just like real paper.
  this->fill(COLOR_ON);
}

void HOT EPaperSSD1677::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->width_ || y >= this->height_ || x < 0 || y < 0)
    return;

  this->x_low_ = clamp_at_most(this->x_low_, x);
  this->x_high_ = clamp_at_least(this->x_high_, x - 1);
  this->y_low_ = clamp_at_most(this->y_low_, y);
  this->y_high_ = clamp_at_least(this->y_high_, y - 1);

  size_t pixel_position = y * this->get_width_controller() + x;
  ;

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
  if (this->x_low_ > this->x_high_ || this->y_low_ > this->y_high_)
    return true;
  // calculate x byte addresses for update
  if (this->current_data_index_ == 0) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    this->transfer_start_time_ = millis();
#endif
    ESP_LOGV(TAG, "Start sending data at %ums", (unsigned) millis());
    const uint16_t x_low = this->x_low_ / 8;
    const uint16_t x_high = (this->x_high_ + 8) / 8;
    const uint16_t y_low = this->y_low_;
    const uint16_t y_high = this->y_high_ + 1;
    uint8_t data[4];
    if (this->transform_ & MIRROR_X) {
      data[0] = this->width_ - x_low * 8 - 1;
      data[1] = (this->width_ - x_low * 8 - 1) >> 8;
      data[2] = this->width_ - x_high * 8;
      data[3] = (this->width_ - x_high * 8) >> 8;
    } else {
      data[0] = x_low * 8;
      data[1] = x_low * 8 >> 8;
      data[2] = (x_high * 8) - 1;
      data[3] = (x_high * 8 - 1) >> 8;
    }
    this->cmd_data(0x4E, data, 2);  // set initial counter
    cmd_data(0x44, data, sizeof(data));
    if (transform_ & MIRROR_Y) {
      data[0] = this->height_ - y_low - 1;
      data[1] = (this->height_ - y_low - 1) >> 8;
      data[2] = this->height_ - y_high;
      data[3] = (this->height_ - y_high) >> 8;
    } else {
      data[0] = y_low;
      data[1] = y_low >> 8;
      data[2] = (y_high - 1);
      data[3] = (y_high - 1) >> 8;
    }
    this->cmd_data(0x4F, data, 2);  // set initial counter
    this->cmd_data(0x45, data, sizeof(data));
    this->command(0x24);
    this->bytes_remaining_ = y_high - y_low;  // actually lines remaining
    this->update_width_ = x_high - x_low;
    this->update_startx_ = x_low;
    this->current_data_index_ = y_low;
    ESP_LOGD(TAG, "Updating with x_low_ %d, x_high_ %d, y_low_ %d, y_high_ %d", this->x_low_, this->x_high_,
             this->y_low_, this->y_high_);
    ESP_LOGD(TAG, "Updating with x_low_ %d, x_high_ %d, y_low_ %d, y_high_ %d", x_low, x_high, y_low, y_high);
    ESP_LOGD(TAG, "Writing %zu bytes at %zu offset", this->bytes_remaining_, this->current_data_index_);
    this->x_low_ = this->width_;
    this->x_high_ = 0;
    this->y_low_ = this->height_;
    this->y_high_ = 0;
  }
  const uint32_t start_time = App.get_loop_component_start_time();
  uint8_t bytes_to_send[this->update_width_];  // fits one line
  while (this->bytes_remaining_ != 0) {
    this->bytes_remaining_--;
    size_t buf_idx = 0;
    size_t data_idx = this->current_data_index_++ * this->get_width_controller() / 8 + this->update_startx_;
    for (uint16_t i = 0; i != this->update_width_; i++)
      bytes_to_send[buf_idx++] = this->buffer_[data_idx++];

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
  this->current_data_index_ = 0;
  ESP_LOGV(TAG, "Sent data in %" PRIu32 " ms", millis() - this->transfer_start_time_);
  return true;
}
}  // namespace esphome::epaper_spi
