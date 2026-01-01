#include "ssd1351_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ssd1351_base {

static const char *const TAG = "ssd1351";

static const uint16_t SSD1351_COLORMASK = 0xffff;
static const uint8_t SSD1351_MAX_CONTRAST = 15;
static const uint8_t SSD1351_BYTESPERPIXEL = 2;
// SSD1351 commands
static const uint8_t SSD1351_SETCOLUMN = 0x15;
static const uint8_t SSD1351_SETROW = 0x75;
static const uint8_t SSD1351_SETREMAP = 0xA0;
static const uint8_t SSD1351_STARTLINE = 0xA1;
static const uint8_t SSD1351_DISPLAYOFFSET = 0xA2;
static const uint8_t SSD1351_DISPLAYOFF = 0xAE;
static const uint8_t SSD1351_DISPLAYON = 0xAF;
static const uint8_t SSD1351_PRECHARGE = 0xB1;
static const uint8_t SSD1351_CLOCKDIV = 0xB3;
static const uint8_t SSD1351_PRECHARGELEVEL = 0xBB;
static const uint8_t SSD1351_VCOMH = 0xBE;
// display controls
static const uint8_t SSD1351_DISPLAYALLOFF = 0xA4;
static const uint8_t SSD1351_DISPLAYALLON = 0xA5;
static const uint8_t SSD1351_NORMALDISPLAY = 0xA6;
static const uint8_t SSD1351_INVERTDISPLAY = 0xA7;
// contrast controls
static const uint8_t SSD1351_CONTRASTABC = 0xC1;
static const uint8_t SSD1351_CONTRASTMASTER = 0xC7;
// memory functions
static const uint8_t SSD1351_WRITERAM = 0x5C;
static const uint8_t SSD1351_READRAM = 0x5D;
// other functions
static const uint8_t SSD1351_FUNCTIONSELECT = 0xAB;
static const uint8_t SSD1351_DISPLAYENHANCE = 0xB2;
static const uint8_t SSD1351_SETVSL = 0xB4;
static const uint8_t SSD1351_SETGPIO = 0xB5;
static const uint8_t SSD1351_PRECHARGE2 = 0xB6;
static const uint8_t SSD1351_SETGRAY = 0xB8;
static const uint8_t SSD1351_USELUT = 0xB9;
static const uint8_t SSD1351_MUXRATIO = 0xCA;
static const uint8_t SSD1351_COMMANDLOCK = 0xFD;
static const uint8_t SSD1351_HORIZSCROLL = 0x96;
static const uint8_t SSD1351_STOPSCROLL = 0x9E;
static const uint8_t SSD1351_STARTSCROLL = 0x9F;

void SSD1351::setup() {
  this->init_internal_(this->get_buffer_length_());

  this->command(SSD1351_COMMANDLOCK);
  this->data(0x12);
  this->command(SSD1351_COMMANDLOCK);
  this->data(0xB1);
  this->command(SSD1351_DISPLAYOFF);
  this->command(SSD1351_CLOCKDIV);
  this->data(0xF1);  // 7:4 = Oscillator Freq, 3:0 = CLK Div Ratio (A[3:0]+1 = 1..16)
  this->command(SSD1351_MUXRATIO);
  this->data(127);
  this->command(SSD1351_DISPLAYOFFSET);
  this->data(0x00);
  this->command(SSD1351_SETGPIO);
  this->data(0x00);
  this->command(SSD1351_FUNCTIONSELECT);
  this->data(0x01);  // internal (diode drop)
  this->command(SSD1351_PRECHARGE);
  this->data(0x32);
  this->command(SSD1351_VCOMH);
  this->data(0x05);
  this->command(SSD1351_NORMALDISPLAY);
  this->command(SSD1351_SETVSL);
  this->data(0xA0);
  this->data(0xB5);
  this->data(0x55);
  this->command(SSD1351_PRECHARGE2);
  this->data(0x01);
  this->command(SSD1351_SETREMAP);
  this->data(0x34);
  this->command(SSD1351_STARTLINE);
  this->data(0x00);
  this->command(SSD1351_CONTRASTABC);
  this->data(0xC8);
  this->data(0x80);
  this->data(0xC8);
  set_brightness(this->brightness_);
  this->fill(Color::BLACK);  // clear display - ensures we do not see garbage at power-on
  this->display();           // ...write buffer, which actually clears the display's memory
  this->turn_on();           // display ON
}
void SSD1351::display() {
  this->command(SSD1351_SETCOLUMN);  // set column address
  this->data(0x00);                  // set column start address
  this->data(0x7F);                  // set column end address
  this->command(SSD1351_SETROW);     // set row address
  this->data(0x00);                  // set row start address
  this->data(0x7F);                  // set last row
  this->command(SSD1351_WRITERAM);
  this->write_display_data();
}
void SSD1351::update() {
  this->do_update_();
  this->display();
}
void SSD1351::set_brightness(float brightness) {
  // validation
  if (brightness > 1) {
    this->brightness_ = 1.0;
  } else if (brightness < 0) {
    this->brightness_ = 0;
  } else {
    this->brightness_ = brightness;
  }
  if (!this->is_ready()) {
    return;  // Component is not yet setup skip the command
  }
  // now write the new brightness level to the display
  this->command(SSD1351_CONTRASTMASTER);
  this->data(int(SSD1351_MAX_CONTRAST * (this->brightness_)));
}
bool SSD1351::is_on() { return this->is_on_; }
void SSD1351::turn_on() {
  this->command(SSD1351_DISPLAYON);
  this->is_on_ = true;
}
void SSD1351::turn_off() {
  this->command(SSD1351_DISPLAYOFF);
  this->is_on_ = false;
}
int SSD1351::get_height_internal() {
  switch (this->model_) {
    case SSD1351_MODEL_128_96:
      return 96;
    case SSD1351_MODEL_128_128:
      return 128;
    default:
      return 0;
  }
}
int SSD1351::get_width_internal() {
  switch (this->model_) {
    case SSD1351_MODEL_128_96:
    case SSD1351_MODEL_128_128:
      return 128;
    default:
      return 0;
  }
}
size_t SSD1351::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) * size_t(SSD1351_BYTESPERPIXEL);
}
void HOT SSD1351::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;
  const uint32_t color565 = display::ColorUtil::color_to_565(color);
  // where should the bits go in the big buffer array? math...
  uint16_t pos = (x + y * this->get_width_internal()) * SSD1351_BYTESPERPIXEL;
  this->buffer_[pos++] = (color565 >> 8) & 0xff;
  this->buffer_[pos] = color565 & 0xff;
}
void SSD1351::fill(Color color) {
  const uint32_t color565 = display::ColorUtil::color_to_565(color);
  const uint8_t color_hi = (color565 >> 8) & 0xff;
  const uint8_t color_lo = color565 & 0xff;
  const int16_t w = this->get_width_internal();
  const int16_t h = this->get_height_internal();

  // Calculate fill region, respecting clipping
  display::Rect fill_rect(0, 0, w, h);
  display::Rect clip = this->get_clipping();
  if (clip.is_set()) {
    fill_rect.shrink(clip);
    if (!fill_rect.is_set())
      return;  // Completely clipped
  }

  // Check if filling entire display
  if (fill_rect.x == 0 && fill_rect.y == 0 && fill_rect.w == w && fill_rect.h == h) {
    for (uint32_t i = 0; i < this->get_buffer_length_(); i += 2) {
      this->buffer_[i] = color_hi;
      this->buffer_[i + 1] = color_lo;
    }
    return;
  }

  // Partial fill - 16-bit RGB565 (2 bytes per pixel, row-major)
  const int16_t x_start = fill_rect.x;
  const int16_t x_end = fill_rect.x + fill_rect.w;
  const int16_t y_start = fill_rect.y;
  const int16_t y_end = fill_rect.y + fill_rect.h;

  for (int16_t y = y_start; y < y_end; y++) {
    uint32_t row_pos = (y * w + x_start) * SSD1351_BYTESPERPIXEL;
    for (int16_t x = x_start; x < x_end; x++) {
      this->buffer_[row_pos++] = color_hi;
      this->buffer_[row_pos++] = color_lo;
    }
  }
}
void SSD1351::init_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(1);
    // Trigger Reset
    this->reset_pin_->digital_write(false);
    delay(10);
    // Wake up
    this->reset_pin_->digital_write(true);
  }
}
const char *SSD1351::model_str_() {
  switch (this->model_) {
    case SSD1351_MODEL_128_96:
      return "SSD1351 128x96";
    case SSD1351_MODEL_128_128:
      return "SSD1351 128x128";
    default:
      return "Unknown";
  }
}

}  // namespace ssd1351_base
}  // namespace esphome
