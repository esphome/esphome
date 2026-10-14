#include "epaper_spi_ssd1680.h"

#include <algorithm>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {
ESPHOME_LOG_TAG(TAG, "epaper_spi.ssd1680");

static const uint8_t LUT_PARTIAL[] PROGMEM = {
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
};

bool EPaperSSD1680::initialise(bool partial) {
  if (!EPaperBase::initialise(partial))
    return false;
  if (partial)
    this->cmd_data(0x32, LUT_PARTIAL, sizeof(LUT_PARTIAL));
  return true;
}

void EPaperSSD1680::setup() {
  EPaperMono::setup();
  if (this->is_failed() || !this->is_using_partial_update_())
    return;
  if (!this->sent_.init(this->buffer_length_))
    ESP_LOGW(TAG, "No memory for the previous frame; partial updates will degrade unchanged areas");
}

void EPaperSSD1680::power_on() {
  this->cmd_data(0x22, {0xE0});
  this->command(0x20);
}

void EPaperSSD1680::power_off() {
  this->cmd_data(0x22, {0x83});
  this->command(0x20);
}

void EPaperSSD1680::set_window() {
  uint16_t x_low_pixels = this->x_low_;
  uint16_t x_high_pixels = this->x_high_;
  uint16_t y_low_pixels = this->y_low_;
  uint16_t y_high_pixels = this->y_high_;
  if (!this->is_using_partial_update_()) {
    x_low_pixels = 0;
    x_high_pixels = this->width_;
    y_low_pixels = 0;
    y_high_pixels = this->height_;
  }

  const uint8_t x_low = x_low_pixels / 8;
  const uint8_t x_high = (x_high_pixels + 7) / 8;
  this->cmd_data(0x44, {x_low, (uint8_t) (x_high - 1)});
  this->cmd_data(0x45, {(uint8_t) y_low_pixels, (uint8_t) (y_low_pixels / 256), (uint8_t) (y_high_pixels - 1),
                        (uint8_t) ((y_high_pixels - 1) / 256)});
  this->cmd_data(0x4E, {x_low});
  this->cmd_data(0x4F, {(uint8_t) y_low_pixels, (uint8_t) (y_low_pixels / 256)});
}

void EPaperSSD1680::deep_sleep() {
  this->cmd_data(0x10, {this->is_using_partial_update_() ? (uint8_t) 0x01 : (uint8_t) 0x03});
}

bool EPaperSSD1680::transfer_data() {
  if (!this->sent_.is_valid())
    return EPaperMono::transfer_data();

  const uint32_t start_time = millis();
  if (this->current_data_index_ == 0) {
    if (this->plane_ == 0) {
      this->x_low_ = 0;
      this->x_high_ = this->width_;
      this->y_low_ = 0;
      this->y_high_ = this->height_;
    }
    this->set_window();
    this->command(this->plane_ == 0 ? 0x26 : 0x24);
  }

  this->start_data_();
  const bool send_previous = this->plane_ == 0 && this->sent_.is_valid();
  const auto &source = send_previous ? this->sent_ : this->buffer_;
  while (this->current_data_index_ != this->buffer_length_) {
    size_t length;
    const uint8_t *data = source.get_span(this->current_data_index_, length);
    length = std::min(length, static_cast<size_t>(MAX_TRANSFER_SIZE));
    this->write_array(data, length);
    if (this->plane_ == 1)
      this->sent_.write(this->current_data_index_, data, length);
    this->current_data_index_ += length;
    if (this->current_data_index_ != this->buffer_length_ && millis() - start_time > MAX_TRANSFER_TIME) {
      this->disable();
      return false;
    }
  }
  this->disable();
  this->current_data_index_ = 0;
  if (this->plane_ == 0) {
    this->plane_ = 1;
    return false;
  }
  this->plane_ = 0;
  return true;
}

void EPaperSSD1680::refresh_screen(bool partial) {
  this->cmd_data(0x22, {partial ? (uint8_t) 0xCC : (uint8_t) 0xF7});
  this->command(0x20);
}

}  // namespace esphome::epaper_spi
