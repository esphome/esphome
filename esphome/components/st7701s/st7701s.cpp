#ifdef USE_ESP32_VARIANT_ESP32S3
#include "st7701s.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7701s {

void ST7701S::setup() {
  esph_log_config(TAG, "Setting up ST7701S");
  this->spi_setup();
  esp_lcd_rgb_panel_config_t config{};
  config.flags.fb_in_psram = 1;
  config.timings.h_res = this->width_;
  config.timings.v_res = this->height_;
  config.timings.hsync_pulse_width = this->hsync_pulse_width_;
  config.timings.hsync_back_porch = this->hsync_back_porch_;
  config.timings.hsync_front_porch = this->hsync_front_porch_;
  config.timings.vsync_pulse_width = this->vsync_pulse_width_;
  config.timings.vsync_back_porch = this->vsync_back_porch_;
  config.timings.vsync_front_porch = this->vsync_front_porch_;
  config.timings.flags.pclk_active_neg = this->pclk_inverted_;
  config.timings.pclk_hz = this->pclk_frequency_;
  config.clk_src = LCD_CLK_SRC_PLL160M;
  config.sram_trans_align = 64;
  config.psram_trans_align = 64;
  size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
  for (size_t i = 0; i != data_pin_count; i++) {
    config.data_gpio_nums[i] = this->data_pins_[i]->get_pin();
  }
  config.data_width = data_pin_count;
  config.disp_gpio_num = -1;
  config.hsync_gpio_num = this->hsync_pin_->get_pin();
  config.vsync_gpio_num = this->vsync_pin_->get_pin();
  config.de_gpio_num = this->de_pin_->get_pin();
  config.pclk_gpio_num = this->pclk_pin_->get_pin();
  esp_err_t err = esp_lcd_new_rgb_panel(&config, &this->handle_);
  if (err != ESP_OK) {
    esph_log_e(TAG, "lcd_new_rgb_panel failed: %s", esp_err_to_name(err));
  }
  ESP_ERROR_CHECK(esp_lcd_panel_reset(this->handle_));
  ESP_ERROR_CHECK(esp_lcd_panel_init(this->handle_));
  this->write_init_sequence_();
  esph_log_config(TAG, "ST7701S setup complete");
}

void ST7701S::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                             display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
  if (w <= 0 || h <= 0)
    return;
  // if color mapping is required, pass the buck.
  // note that endianness is not considered here - it is assumed to match!
  if (bitness != display::COLOR_BITNESS_565) {
    return display::Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset,
                                            x_pad);
  }
  x_start += this->offset_x_;
  y_start += this->offset_y_;
  esp_err_t err;
  // x_ and y_offset are offsets into the source buffer, unrelated to our own offsets into the display.
  if (x_offset == 0 && x_pad == 0 && y_offset == 0) {
    // we could deal here with a non-zero y_offset, but if x_offset is zero, y_offset probably will be so don't bother
    err = esp_lcd_panel_draw_bitmap(this->handle_, x_start, y_start, x_start + w, y_start + h, ptr);
  } else {
    // draw line by line
    auto stride = x_offset + w + x_pad;
    for (int y = 0; y != h; y++) {
      err = esp_lcd_panel_draw_bitmap(this->handle_, x_start, y + y_start, x_start + w, y + y_start + 1,
                                      ptr + ((y + y_offset) * stride + x_offset) * 2);
      if (err != ESP_OK)
        break;
    }
  }
  if (err != ESP_OK)
    esph_log_e(TAG, "lcd_lcd_panel_draw_bitmap failed: %s", esp_err_to_name(err));
}

void ST7701S::draw_pixel_at(int x, int y, Color color) {
  if (!this->get_clipping().inside(x, y))
    return;  // NOLINT

  switch (this->rotation_) {
    case display::DISPLAY_ROTATION_0_DEGREES:
      break;
    case display::DISPLAY_ROTATION_90_DEGREES:
      std::swap(x, y);
      x = this->width_ - x - 1;
      break;
    case display::DISPLAY_ROTATION_180_DEGREES:
      x = this->width_ - x - 1;
      y = this->height_ - y - 1;
      break;
    case display::DISPLAY_ROTATION_270_DEGREES:
      std::swap(x, y);
      y = this->height_ - y - 1;
      break;
  }
  auto pixel = convert_big_endian(display::ColorUtil::color_to_565(color));

  this->draw_pixels_at(x, y, 1, 1, (const uint8_t *) &pixel, display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565, true,
                       0, 0, 0);
  App.feed_wdt();
}

void ST7701S::write_command_(uint8_t value) {
  this->enable();
  if (this->dc_pin_ == nullptr) {
    this->write(value, 9);
  } else {
    this->dc_pin_->digital_write(false);
    this->write_byte(value);
    this->dc_pin_->digital_write(true);
  }
  this->disable();
}

void ST7701S::write_data_(uint8_t value) {
  this->enable();
  if (this->dc_pin_ == nullptr) {
    this->write(value | 0x100, 9);
  } else {
    this->dc_pin_->digital_write(true);
    this->write_byte(value);
  }
  this->disable();
}

/**
 * this relies upon the init sequence being well-formed, which is guaranteed by the Python init code.
 */

void ST7701S::write_sequence_(uint8_t cmd, size_t len, const uint8_t *bytes) {
  this->write_command_(cmd);
  while (len-- != 0)
    this->write_data_(*bytes++);
}

void ST7701S::write_init_sequence_() {
  for (size_t i = 0; i != this->init_sequence_.size();) {
    uint8_t cmd = this->init_sequence_[i++];
    size_t len = this->init_sequence_[i++];
    if (len == ST7701S_DELAY_FLAG) {
      ESP_LOGV(TAG, "Delay %dms", cmd);
      delay(cmd);
    } else {
      this->write_sequence_(cmd, len, &this->init_sequence_[i]);
      i += len;
      ESP_LOGV(TAG, "Command %X, %d bytes", cmd, len);
      if (cmd == SW_RESET_CMD)
        delay(6);
    }
  }
  // st7701 does not appear to support axis swapping
  this->write_sequence_(CMD2_BKSEL, sizeof(CMD2_BK0), CMD2_BK0);
  this->write_command_(SDIR_CMD);  // this is in the BK0 command set
  this->write_data_(this->mirror_x_ ? 0x04 : 0x00);
  uint8_t val = this->color_mode_ == display::COLOR_ORDER_BGR ? 0x08 : 0x00;
  if (this->mirror_y_)
    val |= 0x10;
  this->write_command_(MADCTL_CMD);
  this->write_data_(val);
  ESP_LOGD(TAG, "write MADCTL %X", val);
  this->write_command_(this->invert_colors_ ? INVERT_ON : INVERT_OFF);
  this->set_timeout(120, [this] {
    this->write_command_(SLEEP_OUT);
    this->write_command_(DISPLAY_ON);
  });
}

void ST7701S::dump_config() {
  ESP_LOGCONFIG("", "ST7701S RGB LCD");
  ESP_LOGCONFIG(TAG, "  Height: %u", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %u", this->width_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  DE Pin: ", this->de_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
  for (size_t i = 0; i != data_pin_count; i++)
    ESP_LOGCONFIG(TAG, "  Data pin %d: %s", i, (this->data_pins_[i])->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "  SPI Data rate: %dMHz", (unsigned) (this->data_rate_ / 1000000));
}

}  // namespace st7701s
}  // namespace esphome
#endif  // USE_ESP32_VARIANT_ESP32S3
