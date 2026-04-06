#include "epaper_spi_3in97.h"

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi.3in97";

void EPaper3in97::send_command_(uint8_t value) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(value);
  this->disable();
}

void EPaper3in97::send_data_(uint8_t value) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(value);
  this->disable();
}

bool EPaper3in97::wait_busy_demo_() {
  if (this->busy_pin_ == nullptr) {
    return true;
  }

  delay(100);
  const uint32_t start = millis();
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > 5000) {
      ESP_LOGE(TAG, "Timeout while waiting for busy pin");
      return false;
    }
    delay(10);
  }
  return true;
}

void EPaper3in97::fill(Color color) {
  const bool black = (color.red == 0 && color.green == 0 && color.blue == 0);
  const uint8_t fill = black ? 0x00 : 0xFF;
  for (uint32_t i = 0; i < this->buffer_length_; i++) {
    this->buffer_[i] = fill;
  }
}

void HOT EPaper3in97::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  if (x < 0 || y < 0 || x >= this->get_width_internal() || y >= this->get_height_internal())
    return;

  const uint32_t pos = (x / 8) + y * this->row_width_;
  const uint8_t mask = 0x80 >> (x & 7);
  const bool black = (color.red == 0 && color.green == 0 && color.blue == 0);

  // 0 = black, 1 = white
  if (black) {
    this->buffer_[pos] &= ~mask;
  } else {
    this->buffer_[pos] |= mask;
  }
}

bool EPaper3in97::reset() {
  if (this->reset_pin_ == nullptr) {
    this->mark_failed(LOG_STR("reset pin missing"));
    return false;
  }

  this->reset_pin_->digital_write(true);
  delay(20);
  this->reset_pin_->digital_write(false);
  delay(2);
  this->reset_pin_->digital_write(true);
  delay(20);

  if (!this->wait_busy_demo_()) {
    this->status_set_warning();
    return false;
  }

  this->send_command_(0x12);
  if (!this->wait_busy_demo_()) {
    this->status_set_warning();
    return false;
  }

  return true;
}

bool EPaper3in97::initialise(bool partial) {
  ESP_LOGV(TAG, "Initialise");
  (void) partial;

  this->send_command_(0x18);
  this->send_data_(0x80);

  this->send_command_(0x0C);
  this->send_data_(0xAE);
  this->send_data_(0xC7);
  this->send_data_(0xC3);
  this->send_data_(0xC0);
  this->send_data_(0x80);

  this->send_command_(0x01);
  this->send_data_((this->get_height_internal() - 1) & 0xFF);
  this->send_data_((this->get_height_internal() - 1) >> 8);
  this->send_data_(0x02);

  this->send_command_(0x3C);
  this->send_data_(0x01);

  this->send_command_(0x11);
  this->send_data_(0x01);

  this->send_command_(0x44);
  this->send_data_(0x00);
  this->send_data_(0x00);
  this->send_data_((this->get_width_internal() - 1) & 0xFF);
  this->send_data_((this->get_width_internal() - 1) >> 8);

  this->send_command_(0x45);
  this->send_data_((this->get_height_internal() - 1) & 0xFF);
  this->send_data_((this->get_height_internal() - 1) >> 8);
  this->send_data_(0x00);
  this->send_data_(0x00);

  this->send_command_(0x4E);
  this->send_data_(0x00);
  this->send_data_(0x00);

  this->send_command_(0x4F);
  this->send_data_(0x00);
  this->send_data_(0x00);

  if (!this->wait_busy_demo_()) {
    this->status_set_warning();
    return false;
  }

  return true;
}

bool EPaper3in97::transfer_data() {
  ESP_LOGV(TAG, "Transfer data");

  const uint16_t width = this->row_width_;
  const uint16_t height = this->get_height_internal();

  this->send_command_(0x24);
  for (uint16_t j = 0; j < height; j++) {
    for (uint16_t i = 0; i < width; i++) {
      this->send_data_(this->buffer_[i + j * width]);
    }
    delay(1);
  }

  this->send_command_(0x26);
  for (uint16_t j = 0; j < height; j++) {
    for (uint16_t i = 0; i < width; i++) {
      this->send_data_(this->buffer_[i + j * width]);
    }
    delay(1);
  }

  return true;
}

void EPaper3in97::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh");
  (void) partial;

  this->send_command_(0x22);
  this->send_data_(0xF7);
  this->send_command_(0x20);

  if (!this->wait_busy_demo_()) {
    this->status_set_warning();
  }
}

void EPaper3in97::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->send_command_(0x10);
  this->send_data_(0x01);
  delay(100);
}

}  // namespace esphome::epaper_spi

