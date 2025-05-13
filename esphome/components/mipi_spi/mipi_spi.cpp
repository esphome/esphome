#include "mipi_spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mipi_spi {

void MipiSpi::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MIPI SPI");
  this->spi_setup();
  if (this->dc_pin_ != nullptr) {
    this->dc_pin_->setup();
    this->dc_pin_->digital_write(false);
  }
  for (auto *pin : this->enable_pins_) {
    pin->setup();
    pin->digital_write(true);
  }
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(5);
    this->reset_pin_->digital_write(false);
    delay(5);
    this->reset_pin_->digital_write(true);
  }
  this->bus_width_ = this->parent_->get_bus_width();

  // need to know when the display is ready for SLPOUT command - will be 120ms after reset
  auto when = millis() + 120;
  delay(10);
  size_t index = 0;
  auto &vec = this->init_sequence_;
  while (index != vec.size()) {
    if (vec.size() - index < 2) {
      ESP_LOGE(TAG, "Malformed init sequence");
      this->mark_failed();
      return;
    }
    uint8_t cmd = vec[index++];
    uint8_t x = vec[index++];
    if (x == DELAY_FLAG) {
      ESP_LOGD(TAG, "Delay %dms", cmd);
      delay(cmd);
    } else {
      uint8_t num_args = x & 0x7F;
      if (vec.size() - index < num_args) {
        ESP_LOGE(TAG, "Malformed init sequence");
        this->mark_failed();
        return;
      }
      auto arg_byte = vec[index];
      switch (cmd) {
        case SLEEP_OUT: {
          // are we ready, boots?
          int duration = when - millis();
          if (duration > 0) {
            ESP_LOGD(TAG, "Sleep %dms", duration);
            delay(duration);
          }
        } break;

        case INVERT_ON:
          this->invert_colors_ = true;
          break;
        case MADCTL_CMD:
          this->madctl_ = arg_byte;
          break;
        case PIXFMT:
          this->pixel_mode_ = arg_byte & 0x11 ? PIXEL_MODE_16 : PIXEL_MODE_18;
          break;
        case BRIGHTNESS:
          this->brightness_ = arg_byte;
          break;

        default:
          break;
      }
      const auto *ptr = vec.data() + index;
      ESP_LOGD(TAG, "Command %02X, length %d, byte %02X", cmd, num_args, arg_byte);
      this->write_command_(cmd, ptr, num_args);
      index += num_args;
      if (cmd == SLEEP_OUT)
        delay(10);
    }
  }
  this->setup_complete_ = true;
  if (this->draw_from_origin_)
    check_buffer_();
  ESP_LOGCONFIG(TAG, "MIPI SPI setup complete");
}

void MipiSpi::update() {
  if (!this->setup_complete_ || this->is_failed()) {
    return;
  }
  this->do_update_();
  if (this->buffer_ == nullptr || this->x_low_ > this->x_high_ || this->y_low_ > this->y_high_)
    return;
  ESP_LOGV(TAG, "x_low %d, y_low %d, x_high %d, y_high %d", this->x_low_, this->y_low_, this->x_high_, this->y_high_);
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

void MipiSpi::fill(Color color) {
  if (!this->check_buffer_())
    return;
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->get_width_internal() - 1;
  this->y_high_ = this->get_height_internal() - 1;
  switch (this->color_depth_) {
    case display::COLOR_BITNESS_332: {
      auto new_color = display::ColorUtil::color_to_332(color, display::ColorOrder::COLOR_ORDER_RGB);
      memset(this->buffer_, (uint8_t) new_color, this->buffer_bytes_);
      break;
    }
    default: {
      auto new_color = display::ColorUtil::color_to_565(color);
      if (((uint8_t) (new_color >> 8)) == ((uint8_t) new_color)) {
        // Upper and lower is equal can use quicker memset operation. Takes ~20ms.
        memset(this->buffer_, (uint8_t) new_color, this->buffer_bytes_);
      } else {
        auto *ptr_16 = reinterpret_cast<uint16_t *>(this->buffer_);
        auto len = this->buffer_bytes_ / 2;
        while (len--) {
          *ptr_16++ = new_color;
        }
      }
    }
  }
}

void MipiSpi::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }
  if (!this->check_buffer_())
    return;
  size_t pos = (y * this->width_) + x;
  switch (this->color_depth_) {
    case display::COLOR_BITNESS_332: {
      uint8_t new_color = display::ColorUtil::color_to_332(color);
      if (this->buffer_[pos] == new_color)
        return;
      this->buffer_[pos] = new_color;
      break;
    }

    case display::COLOR_BITNESS_565: {
      auto *ptr_16 = reinterpret_cast<uint16_t *>(this->buffer_);
      uint8_t hi_byte = static_cast<uint8_t>(color.r & 0xF8) | (color.g >> 5);
      uint8_t lo_byte = static_cast<uint8_t>((color.g & 0x1C) << 3) | (color.b >> 3);
      uint16_t new_color = hi_byte | (lo_byte << 8);  // big endian
      if (ptr_16[pos] == new_color)
        return;
      ptr_16[pos] = new_color;
      break;
    }
    default:
      return;
  }
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

void MipiSpi::reset_params_() {
  if (!this->is_ready())
    return;
  this->write_command_(this->invert_colors_ ? INVERT_ON : INVERT_OFF);
  if (this->brightness_.has_value())
    this->write_command_(BRIGHTNESS, this->brightness_.value());
}

void MipiSpi::write_init_sequence_() {
  size_t index = 0;
  auto &vec = this->init_sequence_;
  while (index != vec.size()) {
    if (vec.size() - index < 2) {
      ESP_LOGE(TAG, "Malformed init sequence");
      this->mark_failed();
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
        this->mark_failed();
        return;
      }
      const auto *ptr = vec.data() + index;
      this->write_command_(cmd, ptr, num_args);
      index += num_args;
    }
  }
  this->setup_complete_ = true;
  ESP_LOGCONFIG(TAG, "MIPI SPI setup complete");
}

void MipiSpi::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  ESP_LOGVV(TAG, "Set addr %d/%d, %d/%d", x1, y1, x2, y2);
  uint8_t buf[4];
  x1 += this->offset_width_;
  x2 += this->offset_width_;
  y1 += this->offset_height_;
  y2 += this->offset_height_;
  put16_be(buf, y1);
  put16_be(buf + 2, y2);
  this->write_command_(RASET, buf, sizeof buf);
  put16_be(buf, x1);
  put16_be(buf + 2, x2);
  this->write_command_(CASET, buf, sizeof buf);
}

void MipiSpi::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                             display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  if (!this->setup_complete_ || this->is_failed())
    return;
  if (w <= 0 || h <= 0)
    return;
  if (bitness != this->color_depth_ || big_endian != (this->bit_order_ == spi::BIT_ORDER_MSB_FIRST)) {
    Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);
    return;
  }
  if (this->draw_from_origin_) {
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

void MipiSpi::write_18_from_16_bit_(const uint16_t *ptr, size_t w, size_t h, size_t stride) {
  stride -= w;
  uint8_t transfer_buffer[6 * 256];
  size_t idx = 0;  // index into transfer_buffer
  while (h-- != 0) {
    for (auto x = w; x-- != 0;) {
      auto color_val = *ptr++;
      // deal with byte swapping
      transfer_buffer[idx++] = (color_val & 0xF8);                                       // Blue
      transfer_buffer[idx++] = ((color_val & 0x7) << 5) | ((color_val & 0xE000) >> 11);  // Green
      transfer_buffer[idx++] = (color_val >> 5) & 0xF8;                                  // Red
      if (idx == sizeof(transfer_buffer)) {
        this->write_array(transfer_buffer, idx);
        idx = 0;
      }
    }
    ptr += stride;
  }
  if (idx != 0)
    this->write_array(transfer_buffer, idx);
}

void MipiSpi::write_18_from_8_bit_(const uint8_t *ptr, size_t w, size_t h, size_t stride) {
  stride -= w;
  uint8_t transfer_buffer[6 * 256];
  size_t idx = 0;  // index into transfer_buffer
  while (h-- != 0) {
    for (auto x = w; x-- != 0;) {
      auto color_val = *ptr++;
      transfer_buffer[idx++] = color_val & 0xE0;         // Red
      transfer_buffer[idx++] = (color_val << 3) & 0xE0;  // Green
      transfer_buffer[idx++] = color_val << 6;           // Blue
      if (idx == sizeof(transfer_buffer)) {
        this->write_array(transfer_buffer, idx);
        idx = 0;
      }
    }
    ptr += stride;
  }
  if (idx != 0)
    this->write_array(transfer_buffer, idx);
}

void MipiSpi::write_16_from_8_bit_(const uint8_t *ptr, size_t w, size_t h, size_t stride) {
  stride -= w;
  uint8_t transfer_buffer[6 * 256];
  size_t idx = 0;  // index into transfer_buffer
  while (h-- != 0) {
    for (auto x = w; x-- != 0;) {
      auto color_val = *ptr++;
      transfer_buffer[idx++] = (color_val & 0xE0) | ((color_val & 0x1C) >> 2);
      transfer_buffer[idx++] = (color_val & 0x3) << 3;
      if (idx == sizeof(transfer_buffer)) {
        this->write_array(transfer_buffer, idx);
        idx = 0;
      }
    }
    ptr += stride;
  }
  if (idx != 0)
    this->write_array(transfer_buffer, idx);
}

void MipiSpi::write_to_display_(int x_start, int y_start, int w, int h, const uint8_t *ptr, int x_offset, int y_offset,
                                int x_pad) {
  this->set_addr_window_(x_start, y_start, x_start + w - 1, y_start + h - 1);
  auto stride = x_offset + w + x_pad;
  const auto *offset_ptr = ptr;
  if (this->color_depth_ == display::COLOR_BITNESS_332) {
    offset_ptr += y_offset * stride + x_offset;
  } else {
    stride *= 2;
    offset_ptr += y_offset * stride + x_offset * 2;
  }

  switch (this->bus_width_) {
    case 4:
      this->enable();
      if (x_offset == 0 && x_pad == 0 && y_offset == 0) {
        // we could deal here with a non-zero y_offset, but if x_offset is zero, y_offset probably will be so don't
        // bother
        this->write_cmd_addr_data(8, 0x32, 24, WDATA << 8, ptr, w * h * 2, 4);
      } else {
        this->write_cmd_addr_data(8, 0x32, 24, WDATA << 8, nullptr, 0, 4);
        for (int y = 0; y != h; y++) {
          this->write_cmd_addr_data(0, 0, 0, 0, offset_ptr, w * 2, 4);
          offset_ptr += stride;
        }
      }
      break;

    case 8:
      this->write_command_(WDATA);
      this->enable();
      if (x_offset == 0 && x_pad == 0 && y_offset == 0) {
        this->write_cmd_addr_data(0, 0, 0, 0, ptr, w * h * 2, 8);
      } else {
        for (int y = 0; y != h; y++) {
          this->write_cmd_addr_data(0, 0, 0, 0, offset_ptr, w * 2, 8);
          offset_ptr += stride;
        }
      }
      break;

    default:
      this->write_command_(WDATA);
      this->enable();

      if (this->color_depth_ == display::COLOR_BITNESS_565) {
        // Source buffer is 16-bit RGB565
        if (this->pixel_mode_ == PIXEL_MODE_18) {
          // Convert RGB565 to RGB666
          this->write_18_from_16_bit_(reinterpret_cast<const uint16_t *>(offset_ptr), w, h, stride / 2);
        } else {
          // Direct RGB565 output
          if (x_offset == 0 && x_pad == 0 && y_offset == 0) {
            this->write_array(ptr, w * h * 2);
          } else {
            for (int y = 0; y != h; y++) {
              this->write_array(offset_ptr, w * 2);
              offset_ptr += stride;
            }
          }
        }
      } else {
        // Source buffer is 8-bit RGB332
        if (this->pixel_mode_ == PIXEL_MODE_18) {
          // Convert RGB332 to RGB666
          this->write_18_from_8_bit_(offset_ptr, w, h, stride);
        } else {
          this->write_16_from_8_bit_(offset_ptr, w, h, stride);
        }
        break;
      }
  }
  this->disable();
}

void MipiSpi::write_command_(uint8_t cmd, const uint8_t *bytes, size_t len) {
  ESP_LOGV(TAG, "Command %02X, length %d, bytes %s", cmd, len, format_hex_pretty(bytes, len).c_str());
  if (this->bus_width_ == 4) {
    this->enable();
    this->write_cmd_addr_data(8, 0x02, 24, cmd << 8, bytes, len);
    this->disable();
  } else if (this->bus_width_ == 8) {
    this->dc_pin_->digital_write(false);
    this->enable();
    this->write_cmd_addr_data(0, 0, 0, 0, &cmd, 1, 8);
    this->disable();
    this->dc_pin_->digital_write(true);
    if (len != 0) {
      this->enable();
      this->write_cmd_addr_data(0, 0, 0, 0, bytes, len, 8);
      this->disable();
    }
  } else {
    this->dc_pin_->digital_write(false);
    this->enable();
    this->write_byte(cmd);
    this->disable();
    this->dc_pin_->digital_write(true);
    if (len != 0) {
      if (this->spi_16_) {
        for (size_t i = 0; i != len; i++) {
          this->enable();
          this->write_byte(0);
          this->write_byte(bytes[i]);
          this->disable();
        }
      } else {
        this->enable();
        this->write_array(bytes, len);
        this->disable();
      }
    }
  }
}

void MipiSpi::dump_config() {
  ESP_LOGCONFIG(TAG, "MIPI_SPI Display");
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_);
  ESP_LOGCONFIG(TAG, "  Width: %u", this->width_);
  ESP_LOGCONFIG(TAG, "  Height: %u", this->height_);
  if (this->offset_width_ != 0)
    ESP_LOGCONFIG(TAG, "  Offset width: %u", this->offset_width_);
  if (this->offset_height_ != 0)
    ESP_LOGCONFIG(TAG, "  Offset height: %u", this->offset_height_);
  ESP_LOGCONFIG(TAG, "  Swap X/Y: %s", YESNO(this->madctl_ & MADCTL_MV));
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", YESNO(this->madctl_ & (MADCTL_MX | MADCTL_XFLIP)));
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", YESNO(this->madctl_ & (MADCTL_MY | MADCTL_YFLIP)));
  ESP_LOGCONFIG(TAG, "  Color depth: %d bits", this->color_depth_ == display::COLOR_BITNESS_565 ? 16 : 8);
  ESP_LOGCONFIG(TAG, "  Invert colors: %s", YESNO(this->invert_colors_));
  ESP_LOGCONFIG(TAG, "  Color order: %s", this->madctl_ & MADCTL_BGR ? "BGR" : "RGB");
  ESP_LOGCONFIG(TAG, "  Pixel mode: %s", this->pixel_mode_ == PIXEL_MODE_18 ? "18bit" : "16bit");
  if (this->brightness_.has_value())
    ESP_LOGCONFIG(TAG, "  Brightness: %u", this->brightness_.value());
  if (this->spi_16_)
    ESP_LOGCONFIG(TAG, "  SPI 16bit: YES");
  ESP_LOGCONFIG(TAG, "  Draw rounding: %u", this->draw_rounding_);
  if (this->draw_from_origin_)
    ESP_LOGCONFIG(TAG, "  Draw from origin: YES");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  ESP_LOGCONFIG(TAG, "  SPI Mode: %d", this->mode_);
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", static_cast<unsigned>(this->data_rate_ / 1000000));
  ESP_LOGCONFIG(TAG, "  SPI Bus width: %d", this->bus_width_);
}

}  // namespace mipi_spi
}  // namespace esphome
