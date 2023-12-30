//
// Created by Clyde Stubbs on 29/10/2023.
//
#pragma once

#ifdef USE_ESP_IDF
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/display/display.h"
#include "esp_lcd_panel_ops.h"

#include "esp_lcd_panel_rgb.h"

namespace esphome {
namespace rpi_dpi_rgb {

constexpr static const char *const TAG = "rpi_dpi_rgb";

class RPI_DPI_RGB : public display::Display {
 public:
  void update() override { this->do_update_(); }

  void setup() override {
    esph_log_config(TAG, "Setting up RPI_DPI_RGB");
    esp_lcd_rgb_panel_config_t config{};
    config.flags.fb_in_psram = 1;
    config.num_fbs = 2;
    config.timings.pclk_hz = this->pclk_speed_;
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
    config.bits_per_pixel = data_pin_count;
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
    esph_log_config(TAG, "RPI_DPI_RGB setup complete");
  }

  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override {
    if (w <= 0 || h <= 0)
      return;
    // if color mapping or software rotation is required, pass the buck.
    // note that endianness is not considered here - it is assumed to match!
    if (this->rotation_ != display::DISPLAY_ROTATION_0_DEGREES || bitness != display::COLOR_BITNESS_565) {
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
                                        ptr + ((y + y_offset) * stride + x_offset) * 2);
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
  void set_pclk_speed(uint32_t pclk_speed) { this->pclk_speed_ = pclk_speed; }
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
    auto pixel = convert_big_endian(display::ColorUtil::color_to_565(color));

    this->draw_pixels_at(x, y, 1, 1, (const uint8_t *) &pixel, display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565,
                         true, 0, 0, 0);
    App.feed_wdt();
  }

 protected:
  InternalGPIOPin *de_pin_{nullptr};
  InternalGPIOPin *pclk_pin_{nullptr};
  InternalGPIOPin *hsync_pin_{nullptr};
  InternalGPIOPin *vsync_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *dc_pin_{nullptr};
  InternalGPIOPin *data_pins_[16] = {};
  uint16_t hsync_front_porch_ = 8;
  uint16_t hsync_pulse_width_ = 4;
  uint16_t hsync_back_porch_ = 8;
  uint16_t vsync_front_porch_ = 8;
  uint16_t vsync_pulse_width_ = 4;
  uint16_t vsync_back_porch_ = 8;
  uint32_t pclk_speed_ = 16 * 1000 * 1000;

  bool invert_colors_{};
  display::ColorOrder color_mode_{display::COLOR_ORDER_BGR};
  size_t width_{};
  size_t height_{};
  int16_t offset_x_{0};
  int16_t offset_y_{0};

  esp_lcd_panel_handle_t handle_{};
};

}  // namespace rpi_dpi_rgb
}  // namespace esphome
#endif  // USE_ESP_IDF
