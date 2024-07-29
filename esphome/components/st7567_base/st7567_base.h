#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace st7567_base {

static const uint8_t ST7567_BOOSTER_ON = 0x2C;    // internal power supply on
static const uint8_t ST7567_REGULATOR_ON = 0x2E;  // internal power supply on
static const uint8_t ST7567_POWER_ON = 0x2F;      // internal power supply on

static const uint8_t ST7567_DISPLAY_ON = 0xAF;   // Display ON. Normal Display Mode.
static const uint8_t ST7567_DISPLAY_OFF = 0xAE;  // Display OFF. All SEGs/COMs output with VSS
static const uint8_t ST7567_SET_START_LINE = 0x40;
static const uint8_t ST7567_POWER_CTL = 0x28;
static const uint8_t ST7567_SEG_NORMAL = 0xA0;       //
static const uint8_t ST7567_SEG_REVERSE = 0xA1;      // mirror X axis (horizontal)
static const uint8_t ST7567_COM_NORMAL = 0xC0;       //
static const uint8_t ST7567_COM_REMAP = 0xC8;        // mirror Y axis (vertical)
static const uint8_t ST7567_PIXELS_NORMAL = 0xA4;    // display ram content
static const uint8_t ST7567_PIXELS_ALL_ON = 0xA5;    // all pixels on
static const uint8_t ST7567_INVERT_OFF = 0xA6;       // normal pixels
static const uint8_t ST7567_INVERT_ON = 0xA7;        // inverted pixels
static const uint8_t ST7567_SCAN_START_LINE = 0x40;  // scrolling = 0x40 + (0..63)
static const uint8_t ST7567_COL_ADDR_H = 0x10;       // x pos (0..95) 4 MSB
static const uint8_t ST7567_COL_ADDR_L = 0x00;       // x pos (0..95) 4 LSB
static const uint8_t ST7567_PAGE_ADDR = 0xB0;        // y pos, 8.5 rows (0..8)
static const uint8_t ST7567_BIAS_9 = 0xA2;
static const uint8_t ST7567_CONTRAST = 0x80;  // 0x80 + (0..31)
static const uint8_t ST7567_SET_EV_CMD = 0x81;
static const uint8_t ST7567_SET_EV_PARAM = 0x00;
static const uint8_t ST7567_RESISTOR_RATIO = 0x20;
static const uint8_t ST7567_SW_REFRESH = 0xE2;

class ST7567 : public display::DisplayBuffer {
 public:
  void setup() override;

  void update() override;

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void init_mirror_x(bool mirror_x) { this->mirror_x_ = mirror_x; }
  void init_mirror_y(bool mirror_y) { this->mirror_y_ = mirror_y; }
  void init_invert_colors(bool invert_colors) { this->invert_colors_ = invert_colors; }

  void set_invert_colors(bool invert_colors);  // inversion of screen colors
  void set_contrast(uint8_t val);              // 0..63, 27-30 normal
  void set_brightness(uint8_t val);            // 0..7, 5 normal
  void set_all_pixels_on(bool enable);         // turn on all pixels, this doesn't affect RAM
  void set_scroll(uint8_t line);               // set display start line: for screen scrolling w/o affecting RAM

  bool is_on();
  void turn_on();
  void turn_off();

  void request_refresh();  // from datasheet: It is recommended to use the refresh sequence regularly in a specified
                           // interval.

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void fill(Color color) override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

 protected:
  virtual void command(uint8_t value) = 0;
  virtual void write_display_data() = 0;

  void init_reset_();
  void display_init_();
  void display_init_registers_();
  void display_sw_refresh_();

  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  int get_height_internal() override;
  int get_width_internal() override;
  size_t get_buffer_length_();

  int get_offset_x_() { return mirror_x_ ? 4 : 0; };

  const char *model_str_();

  GPIOPin *reset_pin_{nullptr};
  bool is_on_{false};
  // float contrast_{1.0};
  // float brightness_{1.0};
  uint8_t contrast_{27};
  uint8_t brightness_{5};
  bool mirror_x_{true};
  bool mirror_y_{true};
  bool invert_colors_{false};
  bool all_pixels_on_{false};
  uint8_t start_line_{0};
  bool refresh_requested_{false};
};

}  // namespace st7567_base
}  // namespace esphome
