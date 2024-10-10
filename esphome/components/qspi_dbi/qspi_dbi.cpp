#ifdef USE_ESP_IDF
#include "qspi_dbi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace qspi_dbi {

void QspiDbi::setup() {
  esph_log_config(TAG, "Setting up QSPI_DBI");
  this->spi_setup();
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(true);
  }
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(5);
    this->reset_pin_->digital_write(false);
    delay(5);
    this->reset_pin_->digital_write(true);
  }
  this->set_timeout(120, [this] { this->write_command_(SLEEP_OUT); });
  this->set_timeout(240, [this] { this->write_init_sequence_(); });
}

void QspiDbi::update() {
  if (!this->setup_complete_) {
    return;
  }
  this->do_update_();
  // Start addresses and widths/heights must be divisible by 2 (CASET/RASET restriction in datasheet)
  if (this->x_low_ % 2 == 1) {
    this->x_low_--;
  }
  if (this->x_high_ % 2 == 0) {
    this->x_high_++;
  }
  if (this->y_low_ % 2 == 1) {
    this->y_low_--;
  }
  if (this->y_high_ % 2 == 0) {
    this->y_high_++;
  }
  int w = this->x_high_ - this->x_low_ + 1;
  int h = this->y_high_ - this->y_low_ + 1;
  this->draw_pixels_at(this->x_low_, this->y_low_, w, h, this->buffer_, this->color_mode_, display::COLOR_BITNESS_565,
                       true, this->x_low_, this->y_low_, this->get_width_internal() - w - this->x_low_);
  // invalidate watermarks
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

void QspiDbi::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }
  if (this->buffer_ == nullptr)
    this->init_internal_(this->width_ * this->height_ * 2);
  if (this->is_failed())
    return;
  uint32_t pos = (y * this->width_) + x;
  uint16_t new_color;
  bool updated = false;
  pos = pos * 2;
  new_color = display::ColorUtil::color_to_565(color, display::ColorOrder::COLOR_ORDER_RGB);
  if (this->buffer_[pos] != (uint8_t) (new_color >> 8)) {
    this->buffer_[pos] = (uint8_t) (new_color >> 8);
    updated = true;
  }
  pos = pos + 1;
  new_color = new_color & 0xFF;

  if (this->buffer_[pos] != new_color) {
    this->buffer_[pos] = new_color;
    updated = true;
  }
  if (updated) {
    // low and high watermark may speed up drawing from buffer
    if (x < this->x_low_)
      this->x_low_ = x;
    if (y < this->y_low_)
      this->y_low_ = y;
    if (x > this->x_high_)
      this->x_high_ = x;
    if (y > this->y_high_)
      this->y_high_ = y;
  }
}

void QspiDbi::reset_params_(bool ready) {
  if (!ready && !this->is_ready())
    return;
  this->write_command_(this->invert_colors_ ? INVERT_ON : INVERT_OFF);
  // custom x/y transform and color order
  uint8_t mad = this->color_mode_ == display::COLOR_ORDER_BGR ? MADCTL_BGR : MADCTL_RGB;
  if (this->swap_xy_)
    mad |= MADCTL_MV;
  if (this->mirror_x_)
    mad |= MADCTL_MX;
  if (this->mirror_y_)
    mad |= MADCTL_MY;
  this->write_command_(MADCTL_CMD, &mad, 1);
  this->write_command_(BRIGHTNESS, &this->brightness_, 1);
}

void QspiDbi::write_init_sequence_() {
  if (this->model_ == RM690B0) {
    this->write_command_(PAGESEL, 0x20);
    this->write_command_(MIPI, 0x0A);
    this->write_command_(WRAM, 0x80);
    this->write_command_(SWIRE1, 0x51);
    this->write_command_(SWIRE2, 0x2E);
    this->write_command_(PAGESEL, 0x00);
    this->write_command_(0xC2, 0x00);
    delay(10);
    this->write_command_(TEON, 0x00);
  } else if (this->model_ == RM67162) {
    this->write_command_(PIXFMT, 0x55);
    this->write_command_(BRIGHTNESS, 0);
  }
  if (!this->extra_init_sequence_.empty())
    this->write_sequence_(this->extra_init_sequence_.data());
  this->write_command_(DISPLAY_ON);
  this->reset_params_(true);
  this->setup_complete_ = true;
  esph_log_config(TAG, "QSPI_DBI setup complete");
}

void QspiDbi::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  ESP_LOGD(TAG, "Set addr %d/%d, %d/%d", x1, y1, x2, y2);
  uint8_t buf[4];
  x1 += this->offset_x_;
  x2 += this->offset_x_;
  y1 += this->offset_y_;
  y2 += this->offset_y_;
  put16_be(buf, x1);
  put16_be(buf + 2, x2);
  this->write_command_(CASET, buf, sizeof buf);
  put16_be(buf, y1);
  put16_be(buf + 2, y2);
  this->write_command_(RASET, buf, sizeof buf);
}

void QspiDbi::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                             display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  if (!this->setup_complete_ || this->is_failed())
    return;
  if (w <= 0 || h <= 0)
    return;
  if (bitness != display::COLOR_BITNESS_565 || order != this->color_mode_ ||
      big_endian != (this->bit_order_ == spi::BIT_ORDER_MSB_FIRST)) {
    return display::Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset,
                                            x_pad);
  }
  this->set_addr_window_(x_start, y_start, x_start + w - 1, y_start + h - 1);
  this->enable();
  // x_ and y_offset are offsets into the source buffer, unrelated to our own offsets into the display.
  if (x_offset == 0 && x_pad == 0 && y_offset == 0) {
    // we could deal here with a non-zero y_offset, but if x_offset is zero, y_offset probably will be so don't bother
    this->write_cmd_addr_data(8, 0x32, 24, 0x2C00, ptr, w * h * 2, 4);
  } else {
    auto stride = x_offset + w + x_pad;
    ESP_LOGD(TAG, "x_pad=%d, x_offset/y_offset=%d/%d, w/h=%d/%d, stride=%d", x_pad, x_offset, y_offset, w, h, stride);
    uint16_t cmd = 0x2C00;
    for (int y = 0; y != h; y++) {
      ESP_LOGD(TAG, "byte offset=%d", ((y + y_offset) * stride + x_offset) * 2);
      delay(2);
      this->write_cmd_addr_data(8, 0x32, 24, cmd, ptr + ((y + y_offset) * stride + x_offset) * 2, w * 2, 4);
      cmd = 0x3C00;
    }
  }
  this->disable();
}

void QspiDbi::write_sequence_(const uint8_t *addr) {
  if (addr == nullptr)
    return;
  uint8_t cmd, x, num_args;
  while ((cmd = *addr++) != 0) {
    x = *addr++;
    if (x == DELAY_FLAG) {
      cmd &= 0x7F;
      ESP_LOGV(TAG, "Delay %dms", cmd);
      delay(cmd);
    } else {
      num_args = x & 0x7F;
      ESP_LOGV(TAG, "Command %02X, length %d, bits %02X", cmd, num_args, *addr);
      this->write_command_(cmd, addr, num_args);
      addr += num_args;
      if (x & 0x80) {
        ESP_LOGV(TAG, "Delay 150ms");
        delay(150);  // NOLINT
      }
    }
  }
}

void QspiDbi::dump_config() {
  ESP_LOGCONFIG("", "QSPI_DBI Display");
  ESP_LOGCONFIG(TAG, "  Height: %u", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %u", this->width_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
}

}  // namespace qspi_dbi
}  // namespace esphome
#endif
