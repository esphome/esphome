#include "sh1122_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sh1122_base {

static const char *const TAG = "sh1122";

static const uint8_t SH1122_MAX_CONTRAST = 255;
static const uint8_t SH1122_COLORMASK = 0x0f;
static const uint8_t SH1122_COLORSHIFT = 4;
static const uint8_t SH1122_PIXELSPERBYTE = 2;

static const uint8_t SH1122_SETLOWCOLUMNADDRESS = 0x00;
static const uint8_t SH1122_SETHIGHCOLUMNADDRESS = 0x10;
static const uint8_t SH1122_SETDISCHARGEVSLLEVEL = 0x30;
static const uint8_t SH1122_SETDISPLAYSTARTLINE = 0x40;
static const uint8_t SH1122_SETCONTRASTCURRENT = 0x81;
static const uint8_t SH1122_SETSEGMENTREMAP = 0xA0;
static const uint8_t SH1122_SETDISPLAYOFFON = 0xA4;
static const uint8_t SH1122_SETNORMALDISPLAY = 0xA6;
static const uint8_t SH1122_SETREVERSEDISPLAY = 0xA7;
static const uint8_t SH1122_SETMULTIPLEXRATIO = 0xA8;
static const uint8_t SH1122_DCDCSETTING = 0xAD;
static const uint8_t SH1122_SETDISPLAYOFF = 0xAE;
static const uint8_t SH1122_SETDISPLAYON = 0xAF;
static const uint8_t SH1122_SETROWADDRESS = 0xB0;
static const uint8_t SH1122_SETSCANDIRECTION = 0xC0;
static const uint8_t SH1122_SETDISPLAYOFFSET = 0xD3;
static const uint8_t SH1122_SETCLOCKDIVIDER = 0xD5;
static const uint8_t SH1122_SETDISCHARGEPRECHARGEPERIOD = 0xD9;
static const uint8_t SH1122_SETVCOMDESELECTLEVEL = 0xDB;
static const uint8_t SH1122_SETVSEGMLEVEL = 0xDC;

void SH1122::setup() {
  this->init_internal_(this->get_buffer_length_());

  this->turn_off();
  this->command2(SH1122_SETCLOCKDIVIDER, 0x50);
  this->command2(SH1122_SETMULTIPLEXRATIO, 0x3F);
  this->command2(SH1122_SETDISPLAYOFFSET, 0x00);
  this->command2(SH1122_SETROWADDRESS, 0x00);
  this->command(SH1122_SETDISPLAYSTARTLINE | 32);
  this->command(SH1122_SETDISCHARGEVSLLEVEL);
  this->command2(SH1122_DCDCSETTING, 0x81);
  this->command(SH1122_SETSEGMENTREMAP | 0x01);
  this->command(SH1122_SETSCANDIRECTION | 0x08);
  this->command2(SH1122_SETDISCHARGEPRECHARGEPERIOD, 0x28);
  this->command2(SH1122_SETVCOMDESELECTLEVEL, 0x35);
  this->command2(SH1122_SETVSEGMLEVEL, 0x35);
  this->command(SH1122_SETNORMALDISPLAY);
  this->command(SH1122_SETHIGHCOLUMNADDRESS);
  this->command(SH1122_SETLOWCOLUMNADDRESS);
  this->command(SH1122_SETDISPLAYOFFON);
  set_brightness(this->brightness_);
  this->fill(Color::BLACK);  // clear display - ensures we do not see garbage at power-on
  this->display();           // ...write buffer, which actually clears the display's memory
  this->turn_on();           // display ON
}
void SH1122::display() {
  this->command(SH1122_SETHIGHCOLUMNADDRESS);  // set column address
  this->command(SH1122_SETLOWCOLUMNADDRESS);
  this->command2(SH1122_SETROWADDRESS, 0x00);  // set row address

  this->write_display_data();
}
void SH1122::update() {
  this->do_update_();
  this->display();
}
void SH1122::set_brightness(float brightness) {
  this->brightness_ = clamp(brightness, 0.0F, 1.0F);
  // now write the new brightness level to the display
  this->command2(SH1122_SETCONTRASTCURRENT, int(SH1122_MAX_CONTRAST * (this->brightness_)));
}
bool SH1122::is_on() { return this->is_on_; }
void SH1122::turn_on() {
  this->command(SH1122_SETDISPLAYON);
  this->is_on_ = true;
}
void SH1122::turn_off() {
  this->command(SH1122_SETDISPLAYOFF);
  this->is_on_ = false;
}
int SH1122::get_height_internal() {
  switch (this->model_) {
    case SH1122_MODEL_256_64:
      return 64;
    default:
      return 0;
  }
}
int SH1122::get_width_internal() {
  switch (this->model_) {
    case SH1122_MODEL_256_64:
      return 256;
    default:
      return 0;
  }
}
size_t SH1122::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / SH1122_PIXELSPERBYTE;
}
void HOT SH1122::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;
  uint32_t color4 = display::ColorUtil::color_to_grayscale4(color);
  // where should the bits go in the big buffer array? math...
  uint16_t pos = (x / SH1122_PIXELSPERBYTE) + (y * this->get_width_internal() / SH1122_PIXELSPERBYTE);
  uint8_t shift = (1u - (x % SH1122_PIXELSPERBYTE)) * SH1122_COLORSHIFT;
  // ensure 'color4' is valid (only 4 bits aka 1 nibble) and shift the bits left when necessary
  color4 = (color4 & SH1122_COLORMASK) << shift;
  // first mask off the nibble we must change...
  this->buffer_[pos] &= (~SH1122_COLORMASK >> shift);
  // ...then lay the new nibble back on top. done!
  this->buffer_[pos] |= color4;
}
void SH1122::fill(Color color) {
  const uint32_t color4 = display::ColorUtil::color_to_grayscale4(color);
  uint8_t fill = (color4 & SH1122_COLORMASK) | ((color4 & SH1122_COLORMASK) << SH1122_COLORSHIFT);
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}
void SH1122::init_reset_() {
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
const char *SH1122::model_str_() {
  switch (this->model_) {
    case SH1122_MODEL_256_64:
      return "SH1122 256x64";
    default:
      return "Unknown";
  }
}

}  // namespace sh1122_base
}  // namespace esphome
