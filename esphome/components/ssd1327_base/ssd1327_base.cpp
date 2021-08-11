#include "ssd1327_base.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ssd1327_base {

static const char *const TAG = "ssd1327";

static const uint8_t SSD1327_MAX_CONTRAST = 127;
static const uint8_t SSD1327_COLORMASK = 0x0f;
static const uint8_t SSD1327_COLORSHIFT = 4;
static const uint8_t SSD1327_PIXELSPERBYTE = 2;

static const uint8_t SSD1327_SETCOLUMNADDRESS = 0x15;
static const uint8_t SSD1327_SETROWADDRESS = 0x75;
static const uint8_t SSD1327_SETCONTRAST = 0x81;
static const uint8_t SSD1327_SETREMAP = 0xA0;
static const uint8_t SSD1327_SETSTARTLINE = 0xA1;
static const uint8_t SSD1327_SETOFFSET = 0xA2;
static const uint8_t SSD1327_NORMALDISPLAY = 0xA4;
static const uint8_t SSD1327_DISPLAYALLON = 0xA5;
static const uint8_t SSD1327_DISPLAYALLOFF = 0xA6;
static const uint8_t SSD1327_INVERTDISPLAY = 0xA7;
static const uint8_t SSD1327_SETMULTIPLEX = 0xA8;
static const uint8_t SSD1327_FUNCTIONSELECTIONA = 0xAB;
static const uint8_t SSD1327_DISPLAYOFF = 0xAE;
static const uint8_t SSD1327_DISPLAYON = 0xAF;
static const uint8_t SSD1327_SETPHASELENGTH = 0xB1;
static const uint8_t SSD1327_SETFRONTCLOCKDIVIDER = 0xB3;
static const uint8_t SSD1327_SETGPIO = 0xB5;
static const uint8_t SSD1327_SETSECONDPRECHARGEPERIOD = 0xB6;
static const uint8_t SSD1327_SETGRAYSCALETABLE = 0xB8;
static const uint8_t SSD1327_SELECTDEFAULTLINEARGRAYSCALETABLE = 0xB9;
static const uint8_t SSD1327_SETPRECHARGEVOLTAGE = 0xBC;
static const uint8_t SSD1327_SETVCOMHVOLTAGE = 0xBE;
static const uint8_t SSD1327_FUNCTIONSELECTIONB = 0xD5;
static const uint8_t SSD1327_SETCOMMANDLOCK = 0xFD;
static const uint8_t SSD1327_HORIZONTALSCROLLRIGHTSETUP = 0x26;
static const uint8_t SSD1327_HORIZONTALSCROLLLEFTSETUP = 0x27;
static const uint8_t SSD1327_DEACTIVATESCROLL = 0x2E;
static const uint8_t SSD1327_ACTIVATESCROLL = 0x2F;

void SSD1327::setup() {
  this->init_internal_(this->get_buffer_length_());

  this->turn_off();                             // display OFF
  this->command(SSD1327_SETFRONTCLOCKDIVIDER);  // set osc division
  this->command(0xF1);                          // 145
  this->command(SSD1327_SETMULTIPLEX);          // multiplex ratio
  this->command(0x7f);                          // duty = height - 1
  this->command(SSD1327_SETOFFSET);             // set display offset
  this->command(0x00);                          // 0
  this->command(SSD1327_SETSTARTLINE);          // set start line
  this->command(0x00);                          // ...
  this->command(SSD1327_SETREMAP);              // set segment remapping
  this->command(0x53);                          //  COM bottom-up, split odd/even, enable column and nibble remapping
  this->command(SSD1327_SETGRAYSCALETABLE);
  // gamma ~2.2
  this->command(0);
  this->command(1);
  this->command(2);
  this->command(3);
  this->command(6);
  this->command(8);
  this->command(12);
  this->command(16);
  this->command(20);
  this->command(26);
  this->command(32);
  this->command(39);
  this->command(46);
  this->command(54);
  this->command(63);
  this->command(SSD1327_SETPHASELENGTH);
  this->command(0x55);
  this->command(SSD1327_SETVCOMHVOLTAGE);  // Set High Voltage Level of COM Pin
  this->command(0x1C);
  this->command(SSD1327_NORMALDISPLAY);  // set display mode
  set_brightness(this->brightness_);
  this->fill(Color::BLACK);  // clear display - ensures we do not see garbage at power-on
  this->display();           // ...write buffer, which actually clears the display's memory
  this->turn_on();           // display ON
}
void SSD1327::display() {
  this->command(SSD1327_SETCOLUMNADDRESS);  // set column address
  this->command(0x00);                      // set column start address
  this->command(0x3F);                      // set column end address
  this->command(SSD1327_SETROWADDRESS);     // set row address
  this->command(0x00);                      // set row start address
  this->command(127);                       // set last row

  this->write_display_data();
}
void SSD1327::update() {
  if (!this->is_failed()) {
    this->do_update_();
    this->display();
  }
}
void SSD1327::set_brightness(float brightness) {
  // validation
  this->brightness_ = clamp(brightness, 0.0F, 1.0F);
  // now write the new brightness level to the display
  this->command(SSD1327_SETCONTRAST);
  this->command(int(SSD1327_MAX_CONTRAST * (this->brightness_)));
}
bool SSD1327::is_on() { return this->is_on_; }
void SSD1327::turn_on() {
  this->command(SSD1327_DISPLAYON);
  this->is_on_ = true;
}
void SSD1327::turn_off() {
  this->command(SSD1327_DISPLAYOFF);
  this->is_on_ = false;
}
int SSD1327::get_height_internal() {
  switch (this->model_) {
    case SSD1327_MODEL_128_128:
      return 128;
    default:
      return 0;
  }
}
int SSD1327::get_width_internal() {
  switch (this->model_) {
    case SSD1327_MODEL_128_128:
      return 128;
    default:
      return 0;
  }
}
size_t SSD1327::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / SSD1327_PIXELSPERBYTE;
}
void HOT SSD1327::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;
  uint32_t color4 = display::ColorUtil::color_to_grayscale4(color);
  // where should the bits go in the big buffer array? math...
  uint16_t pos = (x / SSD1327_PIXELSPERBYTE) + (y * this->get_width_internal() / SSD1327_PIXELSPERBYTE);
  uint8_t shift = (x % SSD1327_PIXELSPERBYTE) * SSD1327_COLORSHIFT;
  // ensure 'color4' is valid (only 4 bits aka 1 nibble) and shift the bits left when necessary
  color4 = (color4 & SSD1327_COLORMASK) << shift;
  // first mask off the nibble we must change...
  this->buffer_[pos] &= (~SSD1327_COLORMASK >> shift);
  // ...then lay the new nibble back on top. done!
  this->buffer_[pos] |= color4;
}
void SSD1327::fill(Color color) {
  const uint32_t color4 = display::ColorUtil::color_to_grayscale4(color);
  uint8_t fill = (color4 & SSD1327_COLORMASK) | ((color4 & SSD1327_COLORMASK) << SSD1327_COLORSHIFT);
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}
void SSD1327::init_reset_() {
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
const char *SSD1327::model_str_() {
  switch (this->model_) {
    case SSD1327_MODEL_128_128:
      return "SSD1327 128x128";
    default:
      return "Unknown";
  }
}

}  // namespace ssd1327_base
}  // namespace esphome
