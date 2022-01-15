#include "ssd1322_base.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ssd1322_base {

static const char *const TAG = "ssd1322";

static const uint8_t SSD1322_MAX_CONTRAST = 255;
static const uint8_t SSD1322_COLORMASK = 0x0f;
static const uint8_t SSD1322_COLORSHIFT = 4;
static const uint8_t SSD1322_PIXELSPERBYTE = 2;

static const uint8_t SSD1322_ENABLEGRAYSCALETABLE = 0x00;
static const uint8_t SSD1322_SETCOLUMNADDRESS = 0x15;
static const uint8_t SSD1322_WRITERAM = 0x5C;
static const uint8_t SSD1322_READRAM = 0x5D;
static const uint8_t SSD1322_SETROWADDRESS = 0x75;
static const uint8_t SSD1322_SETREMAP = 0xA0;
static const uint8_t SSD1322_SETSTARTLINE = 0xA1;
static const uint8_t SSD1322_SETOFFSET = 0xA2;
static const uint8_t SSD1322_SETMODEALLOFF = 0xA4;
static const uint8_t SSD1322_SETMODEALLON = 0xA5;
static const uint8_t SSD1322_SETMODENORMAL = 0xA6;
static const uint8_t SSD1322_SETMODEINVERTED = 0xA7;
static const uint8_t SSD1322_ENABLEPARTIALDISPLAY = 0xA8;
static const uint8_t SSD1322_EXITPARTIALDISPLAY = 0xA9;
static const uint8_t SSD1322_SETFUNCTIONSELECTION = 0xAB;
static const uint8_t SSD1322_SETDISPLAYOFF = 0xAE;
static const uint8_t SSD1322_SETDISPLAYON = 0xAF;
static const uint8_t SSD1322_SETPHASELENGTH = 0xB1;
static const uint8_t SSD1322_SETFRONTCLOCKDIVIDER = 0xB3;
static const uint8_t SSD1322_DISPLAYENHANCEMENTA = 0xB4;
static const uint8_t SSD1322_SETGPIO = 0xB5;
static const uint8_t SSD1322_SETSECONDPRECHARGEPERIOD = 0xB6;
static const uint8_t SSD1322_SETGRAYSCALETABLE = 0xB8;
static const uint8_t SSD1322_SELECTDEFAULTLINEARGRAYSCALETABLE = 0xB9;
static const uint8_t SSD1322_SETPRECHARGEVOLTAGE = 0xBB;
static const uint8_t SSD1322_SETVCOMHVOLTAGE = 0xBE;
static const uint8_t SSD1322_SETCONTRAST = 0xC1;
static const uint8_t SSD1322_MASTERCURRENTCONTROL = 0xC7;
static const uint8_t SSD1322_SETMULTIPLEXRATIO = 0xCA;
static const uint8_t SSD1322_DISPLAYENHANCEMENTB = 0xD1;
static const uint8_t SSD1322_SETCOMMANDLOCK = 0xFD;

static const uint8_t SSD1322_SETCOMMANDLOCK_UNLOCK = 0x12;
static const uint8_t SSD1322_SETCOMMANDLOCK_LOCK = 0x16;

void SSD1322::setup() {
  this->init_internal_(this->get_buffer_length_());

  this->command(SSD1322_SETCOMMANDLOCK);
  this->data(SSD1322_SETCOMMANDLOCK_UNLOCK);
  this->turn_off();
  this->command(SSD1322_SETFRONTCLOCKDIVIDER);
  this->data(0x91);
  this->command(SSD1322_SETMULTIPLEXRATIO);
  this->data(0x3F);
  this->command(SSD1322_SETOFFSET);
  this->data(0x00);
  this->command(SSD1322_SETSTARTLINE);
  this->data(0x00);
  this->command(SSD1322_SETREMAP);
  this->data(0x14);
  this->data(0x11);
  this->command(SSD1322_SETGPIO);
  this->data(0x00);
  this->command(SSD1322_SETFUNCTIONSELECTION);
  this->data(0x01);
  this->command(SSD1322_DISPLAYENHANCEMENTA);
  this->data(0xA0);
  this->data(0xFD);
  this->command(SSD1322_MASTERCURRENTCONTROL);
  this->data(0x0F);
  this->command(SSD1322_SETPHASELENGTH);
  this->data(0xE2);
  this->command(SSD1322_DISPLAYENHANCEMENTB);
  this->data(0x82);
  this->data(0x20);
  this->command(SSD1322_SETPRECHARGEVOLTAGE);
  this->data(0x1F);
  this->command(SSD1322_SETSECONDPRECHARGEPERIOD);
  this->data(0x08);
  this->command(SSD1322_SETVCOMHVOLTAGE);
  this->data(0x07);
  this->command(SSD1322_SETMODENORMAL);
  this->command(SSD1322_EXITPARTIALDISPLAY);
  // this->command(SSD1322_SELECTDEFAULTLINEARGRAYSCALETABLE);
  this->command(SSD1322_SETGRAYSCALETABLE);
  // gamma ~2.2
  this->data(24);
  this->data(29);
  this->data(36);
  this->data(43);
  this->data(51);
  this->data(60);
  this->data(70);
  this->data(81);
  this->data(93);
  this->data(105);
  this->data(118);
  this->data(132);
  this->data(147);
  this->data(163);
  this->data(180);
  this->command(SSD1322_ENABLEGRAYSCALETABLE);
  set_brightness(this->brightness_);
  this->fill(Color::BLACK);  // clear display - ensures we do not see garbage at power-on
  this->display();           // ...write buffer, which actually clears the display's memory
  this->turn_on();           // display ON
}
void SSD1322::display() {
  this->command(SSD1322_SETCOLUMNADDRESS);  // set column address
  this->data(0x1C);                         // set column start address
  this->data(0x5B);                         // set column end address
  this->command(SSD1322_SETROWADDRESS);     // set row address
  this->data(0x00);                         // set row start address
  this->data(0x3F);                         // set last row
  this->command(SSD1322_WRITERAM);          // write

  this->write_display_data();
}
void SSD1322::update() {
  this->do_update_();
  this->display();
}
void SSD1322::set_brightness(float brightness) {
  this->brightness_ = clamp(brightness, 0.0F, 1.0F);
  // now write the new brightness level to the display
  this->command(SSD1322_SETCONTRAST);
  this->data(int(SSD1322_MAX_CONTRAST * (this->brightness_)));
}
bool SSD1322::is_on() { return this->is_on_; }
void SSD1322::turn_on() {
  this->command(SSD1322_SETDISPLAYON);
  this->is_on_ = true;
}
void SSD1322::turn_off() {
  this->command(SSD1322_SETDISPLAYOFF);
  this->is_on_ = false;
}
int SSD1322::get_height_internal() {
  switch (this->model_) {
    case SSD1322_MODEL_256_64:
      return 64;
    default:
      return 0;
  }
}
int SSD1322::get_width_internal() {
  switch (this->model_) {
    case SSD1322_MODEL_256_64:
      return 256;
    default:
      return 0;
  }
}
size_t SSD1322::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / SSD1322_PIXELSPERBYTE;
}
void HOT SSD1322::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;
  uint32_t color4 = display::ColorUtil::color_to_grayscale4(color);
  // where should the bits go in the big buffer array? math...
  uint16_t pos = (x / SSD1322_PIXELSPERBYTE) + (y * this->get_width_internal() / SSD1322_PIXELSPERBYTE);
  uint8_t shift = (1u - (x % SSD1322_PIXELSPERBYTE)) * SSD1322_COLORSHIFT;
  // ensure 'color4' is valid (only 4 bits aka 1 nibble) and shift the bits left when necessary
  color4 = (color4 & SSD1322_COLORMASK) << shift;
  // first mask off the nibble we must change...
  this->buffer_[pos] &= (~SSD1322_COLORMASK >> shift);
  // ...then lay the new nibble back on top. done!
  this->buffer_[pos] |= color4;
}
void SSD1322::fill(Color color) {
  const uint32_t color4 = display::ColorUtil::color_to_grayscale4(color);
  uint8_t fill = (color4 & SSD1322_COLORMASK) | ((color4 & SSD1322_COLORMASK) << SSD1322_COLORSHIFT);
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}
void SSD1322::init_reset_() {
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
const char *SSD1322::model_str_() {
  switch (this->model_) {
    case SSD1322_MODEL_256_64:
      return "SSD1322 256x64";
    default:
      return "Unknown";
  }
}

}  // namespace ssd1322_base
}  // namespace esphome
