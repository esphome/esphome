//
// Created by Clyde Stubbs on 29/10/2023.
//
#pragma once

// only applicable on ESP32-P4
#ifdef USE_ESP32_VARIANT_ESP32P4
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/gpio.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/display_color_utils.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"

#include "esp_lcd_mipi_dsi.h"

#include <utility>

namespace esphome::mipi_dsi {

ESPHOME_LOG_TAG(TAG, "display.mipi_dsi");
const uint8_t SW_RESET_CMD = 0x01;
const uint8_t SLEEP_OUT = 0x11;
const uint8_t SDIR_CMD = 0xC7;
const uint8_t MADCTL_CMD = 0x36;
const uint8_t INVERT_OFF = 0x20;
const uint8_t INVERT_ON = 0x21;
const uint8_t DISPLAY_ON = 0x29;
const uint8_t CMD2_BKSEL = 0xFF;
const uint8_t MADCTL_BGR = 0x08;
const uint8_t MADCTL_MX = 0x40;
const uint8_t MADCTL_MY = 0x80;
const uint8_t MADCTL_MV = 0x20;     // row/column swap
const uint8_t MADCTL_XFLIP = 0x02;  // Mirror the display horizontally
const uint8_t MADCTL_YFLIP = 0x01;  // Mirror the display vertically

class MipiDsi final : public display::Display {
 public:
  MipiDsi(size_t width, size_t height, display::ColorBitness color_depth, uint8_t pixel_mode)
      : width_(width), height_(height), color_depth_(color_depth), pixel_mode_(pixel_mode) {}
  void set_big_endian(bool big_endian) { this->big_endian_ = big_endian; }
  void set_invert_colors(bool invert_colors) { this->invert_colors_ = invert_colors; }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_enable_pins(std::vector<GPIOPin *> enable_pins) { this->enable_pins_ = std::move(enable_pins); }
  void set_pclk_frequency(float pclk_frequency) { this->pclk_frequency_ = pclk_frequency; }
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }
  void set_hsync_back_porch(uint16_t hsync_back_porch) { this->hsync_back_porch_ = hsync_back_porch; }
  void set_hsync_front_porch(uint16_t hsync_front_porch) { this->hsync_front_porch_ = hsync_front_porch; }
  void set_hsync_pulse_width(uint16_t hsync_pulse_width) { this->hsync_pulse_width_ = hsync_pulse_width; }
  void set_vsync_pulse_width(uint16_t vsync_pulse_width) { this->vsync_pulse_width_ = vsync_pulse_width; }
  void set_vsync_back_porch(uint16_t vsync_back_porch) { this->vsync_back_porch_ = vsync_back_porch; }
  void set_vsync_front_porch(uint16_t vsync_front_porch) { this->vsync_front_porch_ = vsync_front_porch; }
  void set_init_sequence(const uint8_t *init_sequence, size_t len) {
    this->init_sequence_ = init_sequence;
    this->init_sequence_len_ = len;
  }
  void set_model(const char *model) { this->model_ = model; }
  void set_lane_bit_rate(float lane_bit_rate) { this->lane_bit_rate_ = lane_bit_rate; }
  void set_lanes(uint8_t lanes) { this->lanes_ = lanes; }

  void smark_failed(const LogString *message, esp_err_t err);

  void update() override;

  void setup() override;

  void draw_pixel_at(int x, int y, Color color) override;
  void fill(Color color) override;
  int get_width() override;
  int get_height() override;

  void dump_config() override;

 protected:
  void draw_pixels_internal(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorBitness bitness,
                            bool big_endian, int x_offset, int y_offset, int x_pad) override;

  void write_to_display_(int x_start, int y_start, int w, int h, const uint8_t *ptr, int x_offset, int y_offset,
                         int x_pad);
  bool check_buffer_();
  /// Send the region touched since the last flush to the panel.
  void flush_dirty_();
  // Maps drawn coordinates to the buffer the way draw_pixel_at() stores them
  void rotate_(int &x, int &y) const {
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
  }
  void mark_dirty_(int x_start, int y_start, int w, int h);
  /// Pack a colour as the 565 value the panel's byte order expects in memory.
  uint16_t to_panel_565_(Color color) const {
    const uint16_t rgb565 = display::ColorUtil::color_to_565(color);
    return this->big_endian_ ? convert_big_endian(rgb565) : rgb565;
  }
  GPIOPin *reset_pin_{nullptr};
  std::vector<GPIOPin *> enable_pins_{};
  size_t width_{};
  size_t height_{};
  uint16_t hsync_pulse_width_ = 10;
  uint16_t hsync_back_porch_ = 10;
  uint16_t hsync_front_porch_ = 20;
  uint16_t vsync_pulse_width_ = 10;
  uint16_t vsync_back_porch_ = 10;
  uint16_t vsync_front_porch_ = 10;
  const char *model_{"Unknown"};
  // Shared PROGMEM table
  const uint8_t *init_sequence_{nullptr};
  size_t init_sequence_len_{0};
  float pclk_frequency_ = 16;  // in MHz
  float lane_bit_rate_{1500};  // in Mbps
  uint8_t lanes_{2};           // 1, 2, 3 or 4 lanes

  bool invert_colors_{};
  display::ColorBitness color_depth_;
  bool big_endian_{false};
  bool updating_{false};
  uint8_t pixel_mode_{};

  esp_lcd_panel_handle_t handle_{};
  esp_lcd_dsi_bus_handle_t bus_handle_{};
  esp_lcd_panel_io_handle_t io_handle_{};
  SemaphoreHandle_t io_lock_{};
  uint8_t *buffer_{nullptr};
  uint16_t x_low_{1};
  uint16_t y_low_{1};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
};

}  // namespace esphome::mipi_dsi
#endif
