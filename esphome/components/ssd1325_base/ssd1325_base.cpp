#include "ssd1325_base.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ssd1325_base {

static const char *TAG = "ssd1325";

static const uint8_t BLACK = 0;
static const uint8_t WHITE = 1;

static const uint8_t SSD1325_SETCOLADDR = 0x15;
static const uint8_t SSD1325_SETROWADDR = 0x75;
static const uint8_t SSD1325_SETCONTRAST = 0x81;
static const uint8_t SSD1325_SETCURRENT = 0x84;

static const uint8_t SSD1325_SETREMAP = 0xA0;
static const uint8_t SSD1325_SETSTARTLINE = 0xA1;
static const uint8_t SSD1325_SETOFFSET = 0xA2;
static const uint8_t SSD1325_NORMALDISPLAY = 0xA4;
static const uint8_t SSD1325_DISPLAYALLON = 0xA5;
static const uint8_t SSD1325_DISPLAYALLOFF = 0xA6;
static const uint8_t SSD1325_INVERTDISPLAY = 0xA7;
static const uint8_t SSD1325_SETMULTIPLEX = 0xA8;
static const uint8_t SSD1325_MASTERCONFIG = 0xAD;
static const uint8_t SSD1325_DISPLAYOFF = 0xAE;
static const uint8_t SSD1325_DISPLAYON = 0xAF;

static const uint8_t SSD1325_SETPRECHARGECOMPENABLE = 0xB0;
static const uint8_t SSD1325_SETPHASELEN = 0xB1;
static const uint8_t SSD1325_SETROWPERIOD = 0xB2;
static const uint8_t SSD1325_SETCLOCK = 0xB3;
static const uint8_t SSD1325_SETPRECHARGECOMP = 0xB4;
static const uint8_t SSD1325_SETGRAYTABLE = 0xB8;
static const uint8_t SSD1325_SETPRECHARGEVOLTAGE = 0xBC;
static const uint8_t SSD1325_SETVCOMLEVEL = 0xBE;
static const uint8_t SSD1325_SETVSL = 0xBF;

static const uint8_t SSD1325_GFXACCEL = 0x23;
static const uint8_t SSD1325_DRAWRECT = 0x24;
static const uint8_t SSD1325_COPY = 0x25;

void SSD1325::setup() {
  this->init_internal_(this->get_buffer_length_());

  this->command(SSD1325_DISPLAYOFF);   /* display off */
  this->command(SSD1325_SETCLOCK);     /* set osc division */
  this->command(0xF1);                 /* 145 */
  this->command(SSD1325_SETMULTIPLEX); /* multiplex ratio */
  this->command(0x3f);                 /* duty = 1/64 */
  this->command(SSD1325_SETOFFSET);    /* set display offset --- */
  this->command(0x4C);                 /* 76 */
  this->command(SSD1325_SETSTARTLINE); /*set start line */
  this->command(0x00);                 /* ------ */
  this->command(SSD1325_MASTERCONFIG); /*Set Master Config DC/DC Converter*/
  this->command(0x02);
  this->command(SSD1325_SETREMAP); /* set segment remap------ */
  this->command(0x56);
  this->command(SSD1325_SETCURRENT + 0x2); /* Set Full Current Range */
  this->command(SSD1325_SETGRAYTABLE);
  this->command(0x01);
  this->command(0x11);
  this->command(0x22);
  this->command(0x32);
  this->command(0x43);
  this->command(0x54);
  this->command(0x65);
  this->command(0x76);
  this->command(SSD1325_SETCONTRAST); /* set contrast current */
  this->command(0x7F);                // max!
  this->command(SSD1325_SETROWPERIOD);
  this->command(0x51);
  this->command(SSD1325_SETPHASELEN);
  this->command(0x55);
  this->command(SSD1325_SETPRECHARGECOMP);
  this->command(0x02);
  this->command(SSD1325_SETPRECHARGECOMPENABLE);
  this->command(0x28);
  this->command(SSD1325_SETVCOMLEVEL);  // Set High Voltage Level of COM Pin
  this->command(0x1C);                  //?
  this->command(SSD1325_SETVSL);        // set Low Voltage Level of SEG Pin
  this->command(0x0D | 0x02);
  this->command(SSD1325_NORMALDISPLAY); /* set display mode */
  this->command(SSD1325_DISPLAYON);     /* display ON */
}
void SSD1325::display() {
  this->command(SSD1325_SETCOLADDR); /* set column address */
  this->command(0x00);               /* set column start address */
  this->command(0x3F);               /* set column end address */
  this->command(SSD1325_SETROWADDR); /* set row address */
  this->command(0x00);               /* set row start address */
  this->command(0x3F);               /* set row end address */

  this->write_display_data();
}
void SSD1325::update() {
  this->do_update_();
  this->display();
}
int SSD1325::get_height_internal() {
  switch (this->model_) {
    case SSD1325_MODEL_128_32:
      return 32;
    case SSD1325_MODEL_128_64:
      return 64;
    case SSD1325_MODEL_96_16:
      return 16;
    case SSD1325_MODEL_64_48:
      return 48;
    default:
      return 0;
  }
}
int SSD1325::get_width_internal() {
  switch (this->model_) {
    case SSD1325_MODEL_128_32:
    case SSD1325_MODEL_128_64:
      return 128;
    case SSD1325_MODEL_96_16:
      return 96;
    case SSD1325_MODEL_64_48:
      return 64;
    default:
      return 0;
  }
}
size_t SSD1325::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
}

void HOT SSD1325::draw_absolute_pixel_internal(int x, int y, int color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;

  uint16_t pos = x + (y / 8) * this->get_width_internal();
  uint8_t subpos = y % 8;
  if (color) {
    this->buffer_[pos] |= (1 << subpos);
  } else {
    this->buffer_[pos] &= ~(1 << subpos);
  }
}
void SSD1325::fill(int color) {
  uint8_t fill = color ? 0xFF : 0x00;
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}
void SSD1325::init_reset_() {
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
const char *SSD1325::model_str_() {
  switch (this->model_) {
    case SSD1325_MODEL_128_32:
      return "SSD1325 128x32";
    case SSD1325_MODEL_128_64:
      return "SSD1325 128x64";
    case SSD1325_MODEL_96_16:
      return "SSD1325 96x16";
    case SSD1325_MODEL_64_48:
      return "SSD1325 64x48";
    default:
      return "Unknown";
  }
}

}  // namespace ssd1325_base
}  // namespace esphome
