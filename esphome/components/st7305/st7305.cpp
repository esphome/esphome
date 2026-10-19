#include "st7305.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::st7305 {

ESPHOME_LOG_TAG(TAG, "st7305");

void ST7305::setup() {
  this->spi_setup();
  this->dc_pin_->setup();
  if (this->reset_pin_ != nullptr)
    this->reset_pin_->setup();

  if (!this->init_buffer_())
    return;

  this->reset_();
}

bool ST7305::init_buffer_() {
  // Keep the framebuffer in internal RAM: ESP-IDF SPI DMA cannot read a PSRAM buffer unless
  // the device opts into psram_dma, and boards with PSRAM often enable it.
  RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::PREFER_INTERNAL);
  this->buffer_ = allocator.allocate(this->buffer_length_);
  if (this->buffer_ == nullptr) {
    this->mark_failed(LOG_STR("Buffer allocation failed"));
    return false;
  }
  memset(this->buffer_, 0x00, this->buffer_length_);
  this->clear_dirty_();
  return true;
}

void ST7305::reset_() {
  static constexpr uint32_t RESET_HIGH_MS = 50;
  static constexpr uint32_t RESET_LOW_MS = 20;
  static constexpr uint32_t RESET_SETTLE_MS = 50;
  static constexpr uint32_t SOFT_RESET_MS = 5;

  if (this->reset_pin_ == nullptr) {
    this->enable();
    this->send_command_(0x01);  // software reset
    this->disable();
    this->set_timeout(STARTUP_TIMEOUT_ID, SOFT_RESET_MS, [this]() { this->send_init_sequence_(); });
    return;
  }
  this->reset_pin_->digital_write(true);
  this->set_timeout(STARTUP_TIMEOUT_ID, RESET_HIGH_MS, [this]() {
    this->reset_pin_->digital_write(false);
    this->set_timeout(STARTUP_TIMEOUT_ID, RESET_LOW_MS, [this]() {
      this->reset_pin_->digital_write(true);
      this->set_timeout(STARTUP_TIMEOUT_ID, RESET_SETTLE_MS, [this]() { this->send_init_sequence_(); });
    });
  });
}

void ST7305::send_init_sequence_() {
  while (this->init_index_ < this->init_sequence_length_) {
    if (this->init_sequence_length_ - this->init_index_ < 2) {
      this->mark_failed(LOG_STR("Malformed init sequence"));
      return;
    }
    const uint8_t command = progmem_read_byte(&this->init_sequence_[this->init_index_++]);
    const uint8_t count = progmem_read_byte(&this->init_sequence_[this->init_index_++]);
    if (count == 0xFF) {
      // Resume the sequence after the delay instead of blocking the main loop.
      this->set_timeout(STARTUP_TIMEOUT_ID, command, [this]() { this->send_init_sequence_(); });
      return;
    }
    if (this->init_sequence_length_ - this->init_index_ < count) {
      this->mark_failed(LOG_STR("Malformed init sequence"));
      return;
    }
    this->enable();
    this->send_command_(command);
    if (count != 0) {
      this->dc_pin_->digital_write(true);
      for (uint8_t i = 0; i < count; i++)
        this->write_byte(progmem_read_byte(&this->init_sequence_[this->init_index_ + i]));
    }
    this->disable();
    this->init_index_ += count;
  }
  this->finish_init_();
}

void ST7305::finish_init_() {
  this->write_full_frame_();
  this->clear_dirty_();
  this->ready_ = true;
}

void ST7305::write_full_frame_() {
  const uint8_t caset[] = {this->column_start_, this->column_end_};
  const uint8_t raset[] = {this->row_start_, this->row_end_};
  this->enable();
  this->send_command_(0x2A);
  this->send_data_(caset, sizeof(caset));
  this->send_command_(0x2B);
  this->send_data_(raset, sizeof(raset));
  this->send_command_(0x2C);
  this->send_data_(this->buffer_, this->buffer_length_);
  this->disable();
}

ST7305::Window ST7305::compute_window_() const {
  Window window{};
  if (this->orientation_ == Orientation::ORIENTATION_LANDSCAPE) {
    // RASET rows are x pairs; CASET columns are 12-pixel bands of y blocks.
    window.stride = static_cast<uint16_t>(this->height_ / 4);
    window.outer_start = static_cast<uint16_t>(this->x_low_ >> 1);
    window.outer_end = static_cast<uint16_t>(this->x_high_ >> 1);
    const int inv_min = this->height_ - 1 - this->y_high_;
    const int inv_max = this->height_ - 1 - this->y_low_;
    window.block_start = static_cast<uint16_t>((inv_min >> 2) / 3 * 3);
    window.block_end =
        static_cast<uint16_t>(std::min(((inv_max >> 2) / 3) * 3 + 2, static_cast<int>(window.stride) - 1));
  } else {
    // RASET rows are y pairs; CASET columns are 12-pixel bands of x blocks.
    window.stride = static_cast<uint16_t>(this->width_ / 4);
    window.outer_start = static_cast<uint16_t>(this->y_low_ >> 1);
    window.outer_end = static_cast<uint16_t>(this->y_high_ >> 1);
    const int block_min = this->x_low_ >> 2;
    const int block_max = this->x_high_ >> 2;
    window.block_start = static_cast<uint16_t>(block_min / 3 * 3);
    window.block_end = static_cast<uint16_t>(std::min((block_max / 3) * 3 + 2, static_cast<int>(window.stride) - 1));
  }
  window.caset_start = static_cast<uint8_t>(this->column_start_ + window.block_start / 3);
  window.caset_end = static_cast<uint8_t>(this->column_start_ + window.block_end / 3);
  return window;
}

void ST7305::write_window_(const Window &window) {
  const uint8_t caset[] = {window.caset_start, window.caset_end};
  const uint8_t raset[] = {static_cast<uint8_t>(this->row_start_ + window.outer_start),
                           static_cast<uint8_t>(this->row_start_ + window.outer_end)};
  this->enable();
  this->send_command_(0x2A);
  this->send_data_(caset, sizeof(caset));
  this->send_command_(0x2B);
  this->send_data_(raset, sizeof(raset));
  this->send_command_(0x2C);
  if (window.block_start == 0 && window.block_end == window.stride - 1) {
    // Full-height window: every outer unit's data is contiguous with the next.
    this->send_data_(this->buffer_ + window.outer_start * window.stride,
                     static_cast<size_t>(window.outer_end - window.outer_start + 1) * window.stride);
  } else {
    const size_t length = window.block_end - window.block_start + 1;
    for (uint16_t outer = window.outer_start; outer <= window.outer_end; outer++)
      this->send_data_(this->buffer_ + outer * window.stride + window.block_start, length);
  }
  this->disable();
}

void ST7305::update() {
  if (!this->ready_)
    return;

  this->do_update_();
  if (!this->is_dirty_())
    return;

  const Window window = this->compute_window_();
  if (window.data_length() > this->buffer_length_ / 2) {
    this->write_full_frame_();
  } else {
    this->write_window_(window);
  }
  this->clear_dirty_();
}

void HOT ST7305::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || y < 0 || x >= this->width_ || y >= this->height_)
    return;
  size_t index;
  uint8_t mask;
  if (this->orientation_ == Orientation::ORIENTATION_LANDSCAPE) {
    const int inv_y = this->height_ - 1 - y;
    index = static_cast<size_t>(x >> 1) * (this->height_ / 4) + (inv_y >> 2);
    mask = static_cast<uint8_t>(1 << (7 - (((inv_y & 3) << 1) | (x & 1))));
  } else {
    index = static_cast<size_t>(y >> 1) * (this->width_ / 4) + (x >> 2);
    mask = static_cast<uint8_t>(1 << (7 - (((x & 3) << 1) | (y & 1))));
  }
  // The init sequence enables INVON, so a set bit is white and a clear bit is black.
  uint8_t &value = this->buffer_[index];
  const uint8_t previous = value;
  if (color.is_on()) {
    value |= mask;
  } else {
    value &= ~mask;
  }
  if (value == previous)
    return;
  this->extend_dirty_(x, y, x, y);
}

void ST7305::fill(Color color) {
  if (this->get_clipping().is_set()) {
    display::Display::fill(color);
    return;
  }
  if (this->buffer_ == nullptr)
    return;

  const uint8_t value = color.is_on() ? 0xFF : 0x00;
  for (size_t index = 0; index < this->buffer_length_; index++) {
    if (this->buffer_[index] == value)
      continue;
    this->buffer_[index] = value;
    if (this->orientation_ == Orientation::ORIENTATION_LANDSCAPE) {
      const int stride = this->height_ / 4;
      const int outer = static_cast<int>(index / stride);
      const int block = static_cast<int>(index % stride);
      this->extend_dirty_(outer * 2, this->height_ - 4 - block * 4, outer * 2 + 1, this->height_ - 1 - block * 4);
    } else {
      const int stride = this->width_ / 4;
      const int outer = static_cast<int>(index / stride);
      const int block = static_cast<int>(index % stride);
      this->extend_dirty_(block * 4, outer * 2, block * 4 + 3, outer * 2 + 1);
    }
  }
}

void ST7305::extend_dirty_(int x0, int y0, int x1, int y1) {
  if (x0 < this->x_low_)
    this->x_low_ = x0;
  if (x1 > this->x_high_)
    this->x_high_ = x1;
  if (y0 < this->y_low_)
    this->y_low_ = y0;
  if (y1 > this->y_high_)
    this->y_high_ = y1;
}

void ST7305::dump_config() {
  LOG_DISPLAY("", "ST7305", this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_);
  ESP_LOGCONFIG(TAG, "  Orientation: %s",
                this->orientation_ == Orientation::ORIENTATION_LANDSCAPE ? LOG_STR_LITERAL("landscape")
                                                                         : LOG_STR_LITERAL("portrait"));
  ESP_LOGCONFIG(TAG, "  Window: columns %u..%u, rows %u..%u", this->column_start_, this->column_end_, this->row_start_,
                this->row_end_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::st7305
