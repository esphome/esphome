//
// Created by Clyde Stubbs on 29/10/2023.
//
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display.h"
#include "esp_lcd_panel_ops.h"

#include "esp_lcd_panel_rgb.h"

namespace esphome {
namespace st7701s {

#ifdef USE_ESP_IDF

constexpr static const char *const TAG = "panel_driver.st7701s";
const uint8_t SW_RESET_CMD = 0x01;
const uint8_t SLEEP_OUT = 0x11;
const uint8_t SDIR_CMD = 0xC7;
const uint8_t MADCTL_CMD = 0x36;
const uint8_t INVERT_OFF = 0x20;
const uint8_t INVERT_ON = 0x21;
const uint8_t DISPLAY_ON = 0x29;
const uint8_t CMD2_BKSEL = 0xFF;
const uint8_t CMD2_BK0[5] = {0x77, 0x01, 0x00, 0x00, 0x10};

class ST7701S : public display::Display,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  void update() override { this->do_update_(); }

  void setup() override {
    this->spi_setup();
    esp_lcd_rgb_panel_config_t config{};
    config.flags.fb_in_psram = 1;
    config.num_fbs = 2;
    config.timings.pclk_hz = 8 * 1000 * 1000;
    config.timings.h_res = this->width_;
    config.timings.v_res = this->height_;
    config.timings.hsync_pulse_width = this->hsync_pulse_width_;
    config.timings.hsync_back_porch = this->hsync_back_porch_;
    config.timings.hsync_front_porch = this->hsync_front_porch_;
    config.timings.vsync_pulse_width = this->vsync_pulse_width_;
    config.timings.vsync_back_porch = this->vsync_back_porch_;
    config.timings.vsync_front_porch = this->vsync_front_porch_;
    config.timings.flags.pclk_active_neg = true;
    config.clk_src = LCD_CLK_SRC_PLL160M;
    config.sram_trans_align = 64;
    config.psram_trans_align = 64;
    size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
    for (size_t i = 0; i != data_pin_count; i++) {
      config.data_gpio_nums[i] = this->data_pins_[i]->get_pin();
    }
    config.data_width = data_pin_count;
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
  }

  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override {
    if (w <= 0 || h <= 0)
      return;
    // if color mapping or software rotation is required, pass the buck.
    if (this->rotation_ != display::DISPLAY_ROTATION_0_DEGREES || bitness != display::COLOR_BITNESS_565 ||
        !big_endian) {
      return display::Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset,
                                              y_offset, x_pad);
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
      for (size_t y = 0; y != h; y++) {
        err = esp_lcd_panel_draw_bitmap(this->handle_, x_start, y + y_start, x_start + w, y + y_start + 1,
                                        ptr + (y + y_offset) * stride + x_offset);
        if (err != ESP_OK)
          break;
      }
    }
    if (err != ESP_OK)
      esph_log_e(TAG, "lcd_lcd_panel_draw_bitmap failed: %s", esp_err_to_name(err));
  }

  display::ColorOrder get_color_mode() { return this->color_mode_; }
  void set_color_mode(display::ColorOrder color_mode) { this->color_mode_ = color_mode; }
  void set_invert_colors(bool invert_colors) { this->invert_colors_ = invert_colors; }

  void add_data_pin(InternalGPIOPin *data_pin, size_t index) { this->data_pins_[index] = data_pin; };
  void set_de_pin(InternalGPIOPin *de_pin) { this->de_pin_ = de_pin; }
  void set_pclk_pin(InternalGPIOPin *pclk_pin) { this->pclk_pin_ = pclk_pin; }
  void set_vsync_pin(InternalGPIOPin *vsync_pin) { this->vsync_pin_ = vsync_pin; }
  void set_hsync_pin(InternalGPIOPin *hsync_pin) { this->hsync_pin_ = hsync_pin; }
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_width(uint16_t width) { this->width_ = width; }
  void set_dimensions(uint16_t width, uint16_t height) {
    this->width_ = width;
    this->height_ = height;
  }
  int get_width() override { return this->width_; }
  int get_height() override { return this->height_; }
  void set_hsync_back_porch(uint16_t hsync_back_porch) { this->hsync_back_porch_ = hsync_back_porch; }
  void set_hsync_front_porch(uint16_t hsync_front_porch) { this->hsync_front_porch_ = hsync_front_porch; }
  void set_hsync_pulse_width(uint16_t hsync_pulse_width) { this->hsync_pulse_width_ = hsync_pulse_width; }
  void set_vsync_pulse_width(uint16_t vsync_pulse_width) { this->vsync_pulse_width_ = vsync_pulse_width; }
  void set_vsync_back_porch(uint16_t vsync_back_porch) { this->vsync_back_porch_ = vsync_back_porch; }
  void set_vsync_front_porch(uint16_t vsync_front_porch) { this->vsync_front_porch_ = vsync_front_porch; }
  void set_init_sequence(const std::vector<uint8_t> &init_sequence) { this->init_sequence_ = init_sequence; }
  void set_mirror_x(bool mirror_x) { this->mirror_x_ = mirror_x; }
  void set_mirror_y(bool mirror_y) { this->mirror_y_ = mirror_y; }
  void set_offsets(int16_t offset_x, int16_t offset_y) {
    this->offset_x_ = offset_x;
    this->offset_y_ = offset_y;
  }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  void dump_config() override;

  // this will be horribly slow.
  void draw_pixel_at(int x, int y, Color color) override {
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
    auto pixel = display::ColorUtil::color_to_565(color);
    this->draw_pixels_at(x, y, 1, 1, (const uint8_t *) &pixel, display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565,
                         true, 0, 0, 0);
    App.feed_wdt();
  }

 protected:
  void write_command_(uint8_t value) {
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

  void write_data_(uint8_t value) {
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

  void write_sequence_(uint8_t cmd, size_t len, const uint8_t *bytes) {
    this->write_command_(cmd);
    while (len-- != 0)
      this->write_data_(*bytes++);
  }

  void write_init_sequence_() {
    for (size_t i = 0; i != this->init_sequence_.size();) {
      uint8_t cmd = this->init_sequence_[i++];
      size_t len = this->init_sequence_[i++];
      this->write_sequence_(cmd, len, &this->init_sequence_[i]);
      i += len;
      esph_log_v(TAG, "Command %X, %d bytes", cmd, len);
      if (cmd == SW_RESET_CMD)
        delay(6);
    }
    // st7701 does not appear to support axis swapping
    this->write_sequence_(CMD2_BKSEL, sizeof(CMD2_BK0), CMD2_BK0);
    this->write_command_(SDIR_CMD);  // this is in the BK0 command set
    this->write_data_(this->mirror_x_ ? 0x04 : 0x00);
    uint8_t val = this->color_mode_ == display::COLOR_ORDER_BGR ? 0x80 : 0x00;
    if (this->mirror_y_)
      val |= 0x10;
    this->write_command_(MADCTL_CMD);
    this->write_data_(val);
    this->write_command_(this->invert_colors_ ? INVERT_ON : INVERT_OFF);
    this->set_timeout(120, [this] {
      this->write_command_(SLEEP_OUT);
      this->write_command_(DISPLAY_ON);
    });
  }

  InternalGPIOPin *de_pin_{nullptr};
  InternalGPIOPin *pclk_pin_{nullptr};
  InternalGPIOPin *hsync_pin_{nullptr};
  InternalGPIOPin *vsync_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
  InternalGPIOPin *data_pins_[16] = {};
  uint16_t hsync_pulse_width_ = 10;
  uint16_t hsync_back_porch_ = 10;
  uint16_t hsync_front_porch_ = 20;
  uint16_t vsync_pulse_width_ = 10;
  uint16_t vsync_back_porch_ = 10;
  uint16_t vsync_front_porch_ = 10;
  std::vector<uint8_t> init_sequence_;

  bool invert_colors_{};
  display::ColorOrder color_mode_{display::COLOR_ORDER_BGR};
  size_t width_{};
  size_t height_{};
  int16_t offset_x_{0};
  int16_t offset_y_{0};
  bool mirror_x_{};
  bool mirror_y_{};

  esp_lcd_panel_handle_t handle_{};
};

#endif
}  // namespace st7701s
}  // namespace esphome
