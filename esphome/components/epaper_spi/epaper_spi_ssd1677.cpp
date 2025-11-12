#include "epaper_spi_ssd1677.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.ssd1677";

static uint8_t color_to_bit(Color color) {
  // It's always a shade of gray. Map to BLACK or WHITE.
  // We split the luminance at a suitable point
  if ((static_cast<int>(color.r) + color.g + color.b) > 512) {
    return 1;
  }
  return 0;
}

void EPaperSSD1677::refresh_screen() {
  static uint8_t x = 0;
  ESP_LOGV(TAG, "Refresh screen");
  this->command(0x22);
  this->data((x++ & 7) == 0 ? 0xF7 : 0xF7);
  this->command(0x20);
}

void EPaperSSD1677::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
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

  // We store 8 pixels per byte
  this->buffer_.fill(pixel_color);
}

void EPaperSSD1677::clear() {
  // clear buffer to white, just like real paper.
  this->fill(COLOR_ON);
}

void HOT EPaperSSD1677::draw_pixel_at(int x, int y, Color color) {
  if (!this->get_clipping().inside(x, y))
    return;
  if (this->transform_ & SWAP_XY)
    std::swap(x, y);
  if (this->transform_ & MIRROR_X)
    x = this->width_ - x - 1;
  if (this->transform_ & MIRROR_Y)
    y = this->height_ - y - 1;
  if (x >= this->width_ || y >= this->height_ || x < 0 || y < 0)
    return;
  const size_t pixel_position = y * this->width_ + x;
  const size_t byte_position = pixel_position / 8;
  const uint8_t bit_position = pixel_position % 8;
  const uint8_t pixel_bit = 0x80 >> bit_position;
  const auto original = this->buffer_[byte_position];
  if ((color_to_bit(color) == 0)) {
    this->buffer_[byte_position] = original & ~pixel_bit;
  } else {
    this->buffer_[byte_position] = original | pixel_bit;
  }
}

bool HOT EPaperSSD1677::transfer_data() {
  auto start_time = millis();
  if (this->current_data_index_ == 0) {
    uint8_t data[4]{};
    data[2] = this->width_ - 1;
    data[3] = (this->width_ - 1) / 256;
    cmd_data(0x4E, data, 2);
    cmd_data(0x44, data, sizeof(data));
    data[2] = this->height_ - 1;
    data[3] = (this->height_ - 1) / 256;
    cmd_data(0x4F, data, 2);
    this->cmd_data(0x45, data, sizeof(data));
    this->command(0x24);
  }
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  size_t buf_idx = 0;
  ESP_LOGV(TAG, "Writing bytes at offset %zu at %ums", this->current_data_index_, (unsigned) millis());
  this->start_data_();
  while (this->current_data_index_ != this->buffer_length_) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];
    if (buf_idx == sizeof(bytes_to_send)) {
      this->write_array(bytes_to_send, buf_idx);
      buf_idx = 0;
      if (millis() - start_time > MAX_TRANSFER_TIME) {
        // Let the main loop run and come back next loop
        this->end_data_();
        return false;
      }
    }
  }
  if (buf_idx != 0) {
    this->write_array(bytes_to_send, buf_idx);
    ESP_LOGV(TAG, "Wrote %d bytes at %ums", buf_idx, (unsigned) millis());
  }
  this->end_data_();
  this->current_data_index_ = 0;
  return true;
}
}  // namespace esphome::epaper_spi
