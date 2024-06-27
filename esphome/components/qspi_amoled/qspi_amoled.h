//
// Created by Clyde Stubbs on 29/10/2023.
//
#pragma once

#ifdef USE_ESP_IDF
#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/display_color_utils.h"
#include "esp_lcd_panel_ops.h"

#include "esp_lcd_panel_rgb.h"

namespace esphome {
namespace qspi_amoled {

constexpr static const char *const TAG = "display.qspi_amoled";
static const uint8_t SW_RESET_CMD = 0x01;
static const uint8_t SLEEP_OUT = 0x11;
static const uint8_t INVERT_OFF = 0x20;
static const uint8_t INVERT_ON = 0x21;
static const uint8_t ALL_ON = 0x23;
static const uint8_t WRAM = 0x24;
static const uint8_t MIPI = 0x26;
static const uint8_t DISPLAY_ON = 0x29;
static const uint8_t RASET = 0x2B;
static const uint8_t CASET = 0x2A;
static const uint8_t WDATA = 0x2C;
static const uint8_t TEON = 0x35;
static const uint8_t MADCTL_CMD = 0x36;
static const uint8_t PIXFMT = 0x3A;
static const uint8_t BRIGHTNESS = 0x51;
static const uint8_t SWIRE1 = 0x5A;
static const uint8_t SWIRE2 = 0x5B;
static const uint8_t PAGESEL = 0xFE;

static const uint8_t MADCTL_MY = 0x80;   ///< Bit 7 Bottom to top
static const uint8_t MADCTL_MX = 0x40;   ///< Bit 6 Right to left
static const uint8_t MADCTL_MV = 0x20;   ///< Bit 5 Reverse Mode
static const uint8_t MADCTL_RGB = 0x00;  ///< Bit 3 Red-Green-Blue pixel order
static const uint8_t MADCTL_BGR = 0x08;  ///< Bit 3 Blue-Green-Red pixel order

// store a 16 bit value in a buffer, big endian.
static inline void put16_be(uint8_t *buf, uint16_t value) {
  buf[0] = value >> 8;
  buf[1] = value;
}

enum Model {
  RM690B0,
  RM67162,
};

class QspiAmoLed : public display::DisplayBuffer,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                         spi::DATA_RATE_1MHZ> {
 public:
  void set_model(Model model) { this->model_ = model; }
  void update() override;
  void setup() override;
  display::ColorOrder get_color_mode() { return this->color_mode_; }
  void set_color_mode(display::ColorOrder color_mode) { this->color_mode_ = color_mode; }

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_enable_pin(GPIOPin *enable_pin) { this->enable_pin_ = enable_pin; }
  void set_dimensions(uint16_t width, uint16_t height) {
    this->width_ = width;
    this->height_ = height;
  }
  void set_invert_colors(bool invert_colors) {
    this->invert_colors_ = invert_colors;
    this->reset_params_();
  }
  void set_mirror_x(bool mirror_x) {
    this->mirror_x_ = mirror_x;
    this->reset_params_();
  }
  void set_mirror_y(bool mirror_y) {
    this->mirror_y_ = mirror_y;
    this->reset_params_();
  }
  void set_swap_xy(bool swap_xy) {
    this->swap_xy_ = swap_xy;
    this->reset_params_();
  }
  void set_brightness(uint8_t brightness) {
    this->brightness_ = brightness;
    this->reset_params_();
  }
  void set_offsets(int16_t offset_x, int16_t offset_y) {
    this->offset_x_ = offset_x;
    this->offset_y_ = offset_y;
  }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  void dump_config() override;

  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }
  bool can_proceed() override { return this->setup_complete_; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;
  /**
   * the RM67162 in quad SPI mode seems to work like this (not in the datasheet, this is deduced from the
   * sample code.)
   *
   * Immediately after enabling /CS send 4 bytes in single-dataline SPI mode:
   *    0: either 0x2 or 0x32. The first indicates that any subsequent data bytes after the initial 4 will be
   *        sent in 1-dataline SPI. The second indicates quad mode.
   *    1: 0x00
   *    2: The command (register address) byte.
   *    3: 0x00
   *
   *    This is followed by zero or more data bytes in either 1-wire or 4-wire mode, depending on the first byte.
   *    At the conclusion of the write, de-assert /CS.
   *
   * @param cmd
   * @param bytes
   * @param len
   */
  void write_command_(uint8_t cmd, const uint8_t *bytes, size_t len) {
    this->enable();
    this->write_cmd_addr_data(8, 0x02, 24, cmd << 8, bytes, len);
    this->disable();
  }

  void write_command_(uint8_t cmd, uint8_t data) { this->write_command_(cmd, &data, 1); }
  void write_command_(uint8_t cmd) { this->write_command_(cmd, &cmd, 0); }
  void reset_params_(bool ready = false);
  void write_init_sequence_();
  void set_addr_window_(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *enable_pin_{nullptr};
  uint16_t x_low_{0};
  uint16_t y_low_{0};
  uint16_t x_high_{0};
  uint16_t y_high_{0};
  bool setup_complete_{};

  bool invert_colors_{};
  display::ColorOrder color_mode_{display::COLOR_ORDER_BGR};
  size_t width_{};
  size_t height_{};
  int16_t offset_x_{0};
  int16_t offset_y_{0};
  bool swap_xy_{};
  bool mirror_x_{};
  bool mirror_y_{};
  uint8_t brightness_{0xD0};
  Model model_{RM690B0};

  esp_lcd_panel_handle_t handle_{};
};

}  // namespace qspi_amoled
}  // namespace esphome
#endif
