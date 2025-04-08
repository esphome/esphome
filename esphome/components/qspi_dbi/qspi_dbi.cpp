#if defined(USE_ESP_IDF) && defined(USE_ESP32_VARIANT_ESP32S3)
#include "qspi_dbi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace qspi_dbi {

void QspiDbi::setup() {
  ESP_LOGCONFIG(TAG, "Setting up QSPI_DBI");
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
  if (this->draw_from_origin_)
    check_buffer_();
}

void QspiDbi::update() {
  if (!this->setup_complete_) {
    return;
  }
  this->do_update_();
  if (this->buffer_ == nullptr || this->x_low_ > this->x_high_ || this->y_low_ > this->y_high_)
    return;
  // Some chips require that the drawing window be aligned on certain boundaries
  auto dr = this->draw_rounding_;
  this->x_low_ = this->x_low_ / dr * dr;
  this->y_low_ = this->y_low_ / dr * dr;
  this->x_high_ = (this->x_high_ + dr) / dr * dr - 1;
  this->y_high_ = (this->y_high_ + dr) / dr * dr - 1;
  if (this->draw_from_origin_) {
    this->x_low_ = 0;
    this->y_low_ = 0;
    this->x_high_ = this->width_ - 1;
  }
  int w = this->x_high_ - this->x_low_ + 1;
  int h = this->y_high_ - this->y_low_ + 1;
  this->write_to_display_(this->x_low_, this->y_low_, w, h, this->buffer_, this->x_low_, this->y_low_,
                          this->width_ - w - this->x_low_);
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
  if (this->is_failed())
    return;
  check_buffer_();
  uint32_t pos = (y * this->width_) + x;
  bool updated = false;
  pos = pos * 2;
  uint16_t new_color = display::ColorUtil::color_to_565(color, display::ColorOrder::COLOR_ORDER_RGB);
  if (this->buffer_[pos] != static_cast<uint8_t>(new_color >> 8)) {
    this->buffer_[pos] = static_cast<uint8_t>(new_color >> 8);
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
  this->write_command_(MADCTL_CMD, mad);
  this->write_command_(BRIGHTNESS, this->brightness_);
  this->write_command_(DISPLAY_ON);
}

void QspiDbi::write_init_sequence_() {
  for (const auto &seq : this->init_sequences_) {
    this->write_sequence_(seq);
  }
  this->reset_params_(true);
  this->setup_complete_ = true;
  ESP_LOGCONFIG(TAG, "QSPI_DBI setup complete");
}

void QspiDbi::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  ESP_LOGVV(TAG, "Set addr %d/%d, %d/%d", x1, y1, x2, y2);
  uint8_t buf[4];
  x1 += this->offset_x_;
  x2 += this->offset_x_;
  y1 += this->offset_y_;
  y2 += this->offset_y_;
  put16_be(buf, y1);
  put16_be(buf + 2, y2);
  this->write_command_(RASET, buf, sizeof buf);
  put16_be(buf, x1);
  put16_be(buf + 2, x2);
  this->write_command_(CASET, buf, sizeof buf);
}

void QspiDbi::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                             display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  if (!this->setup_complete_ || this->is_failed())
    return;
  if (w <= 0 || h <= 0)
    return;
  if (bitness != display::COLOR_BITNESS_565 || order != this->color_mode_ ||
      big_endian != (this->bit_order_ == spi::BIT_ORDER_MSB_FIRST)) {
    Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);
    return;
  } else if (this->draw_from_origin_) {
    auto stride = x_offset + w + x_pad;
    for (int y = 0; y != h; y++) {
      memcpy(this->buffer_ + ((y + y_start) * this->width_ + x_start) * 2,
             ptr + ((y + y_offset) * stride + x_offset) * 2, w * 2);
    }
    ptr = this->buffer_;
    w = this->width_;
    h += y_start;
    x_start = 0;
    y_start = 0;
    x_offset = 0;
    y_offset = 0;
  }
  this->write_to_display_(x_start, y_start, w, h, ptr, x_offset, y_offset, x_pad);
}

void QspiDbi::write_to_display_(int x_start, int y_start, int w, int h, const uint8_t *ptr, int x_offset, int y_offset,
                                int x_pad) {
  this->set_addr_window_(x_start, y_start, x_start + w - 1, y_start + h - 1);
  this->enable();
  // x_ and y_offset are offsets into the source buffer, unrelated to our own offsets into the display.
  if (x_offset == 0 && x_pad == 0 && y_offset == 0) {
    // we could deal here with a non-zero y_offset, but if x_offset is zero, y_offset probably will be so don't bother
    this->write_cmd_addr_data(8, 0x32, 24, 0x2C00, ptr, w * h * 2, 4);
  } else {
    auto stride = x_offset + w + x_pad;
    this->write_cmd_addr_data(8, 0x32, 24, 0x2C00, nullptr, 0, 4);
    for (int y = 0; y != h; y++) {
      this->write_cmd_addr_data(0, 0, 0, 0, ptr + ((y + y_offset) * stride + x_offset) * 2, w * 2, 4);
    }
  }
  this->disable();
}
void QspiDbi::write_command_(uint8_t cmd, const uint8_t *bytes, size_t len) {
  ESP_LOGV(TAG, "Command %02X, length %d, bytes %s", cmd, len, format_hex_pretty(bytes, len).c_str());
  this->enable();
  this->write_cmd_addr_data(8, 0x02, 24, cmd << 8, bytes, len);
  this->disable();
}

void QspiDbi::write_sequence_(const std::vector<uint8_t> &vec) {
  size_t index = 0;
  while (index != vec.size()) {
    if (vec.size() - index < 2) {
      ESP_LOGE(TAG, "Malformed init sequence");
      return;
    }
    uint8_t cmd = vec[index++];
    uint8_t x = vec[index++];
    if (x == DELAY_FLAG) {
      ESP_LOGV(TAG, "Delay %dms", cmd);
      delay(cmd);
    } else {
      uint8_t num_args = x & 0x7F;
      if (vec.size() - index < num_args) {
        ESP_LOGE(TAG, "Malformed init sequence");
        return;
      }
      const auto *ptr = vec.data() + index;
      this->write_command_(cmd, ptr, num_args);
      index += num_args;
    }
  }
}

void QspiDbi::dump_config() {
  ESP_LOGCONFIG("", "QSPI_DBI Display");
  ESP_LOGCONFIG("", "Model: %s", this->model_);
  ESP_LOGCONFIG(TAG, "  Height: %u", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %u", this->width_);
  ESP_LOGCONFIG(TAG, "  Draw rounding: %u", this->draw_rounding_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
}

}  // namespace qspi_dbi
}  // namespace esphome
#endif
