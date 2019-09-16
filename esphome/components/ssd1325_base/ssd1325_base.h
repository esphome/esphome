#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/display/display_buffer.h"

#define BLACK 0
#define WHITE 1

#define SSD1325_SETCOLADDR 0x15
#define SSD1325_SETROWADDR 0x75
#define SSD1325_SETCONTRAST 0x81
#define SSD1325_SETCURRENT 0x84

#define SSD1325_SETREMAP 0xA0
#define SSD1325_SETSTARTLINE 0xA1
#define SSD1325_SETOFFSET 0xA2
#define SSD1325_NORMALDISPLAY 0xA4
#define SSD1325_DISPLAYALLON 0xA5
#define SSD1325_DISPLAYALLOFF 0xA6
#define SSD1325_INVERTDISPLAY 0xA7
#define SSD1325_SETMULTIPLEX 0xA8
#define SSD1325_MASTERCONFIG 0xAD
#define SSD1325_DISPLAYOFF 0xAE
#define SSD1325_DISPLAYON 0xAF

#define SSD1325_SETPRECHARGECOMPENABLE 0xB0
#define SSD1325_SETPHASELEN 0xB1
#define SSD1325_SETROWPERIOD 0xB2
#define SSD1325_SETCLOCK 0xB3
#define SSD1325_SETPRECHARGECOMP 0xB4
#define SSD1325_SETGRAYTABLE 0xB8
#define SSD1325_SETPRECHARGEVOLTAGE 0xBC
#define SSD1325_SETVCOMLEVEL 0xBE
#define SSD1325_SETVSL 0xBF

#define SSD1325_GFXACCEL 0x23
#define SSD1325_DRAWRECT 0x24
#define SSD1325_COPY 0x25

namespace esphome {
namespace ssd1325_base {

enum SSD1325Model {
  SSD1325_MODEL_128_32 = 0,
  SSD1325_MODEL_128_64,
  SSD1325_MODEL_96_16,
  SSD1325_MODEL_64_48,
};

class SSD1325 : public PollingComponent, public display::DisplayBuffer {
 public:
  void setup() override;

  void display();

  void update() override;

  void set_model(SSD1325Model model) { this->model_ = model; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_external_vcc(bool external_vcc) { this->external_vcc_ = external_vcc; }

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }
  void fill(int color) override;

 protected:
  virtual void command(uint8_t value) = 0;
  virtual void write_display_data() = 0;
  void init_reset_();

  bool is_sh1106_() const;

  void draw_absolute_pixel_internal(int x, int y, int color) override;

  int get_height_internal() override;
  int get_width_internal() override;
  size_t get_buffer_length_();
  const char *model_str_();

  SSD1325Model model_{SSD1325_MODEL_128_64};
  GPIOPin *reset_pin_{nullptr};
  bool external_vcc_{false};
};

}  // namespace ssd1325_base
}  // namespace esphome
