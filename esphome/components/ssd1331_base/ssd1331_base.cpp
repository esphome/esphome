#include "ssd1331_base.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ssd1331_base {

static const char *const TAG = "ssd1331";

static const uint16_t SSD1331_COLORMASK = 0xffff;
static const uint8_t SSD1331_MAX_CONTRASTA = 0x91;
static const uint8_t SSD1331_MAX_CONTRASTB = 0x50;
static const uint8_t SSD1331_MAX_CONTRASTC = 0x7D;
static const uint8_t SSD1331_BYTESPERPIXEL = 2;
// SSD1331 Commands
static const uint8_t SSD1331_DRAWLINE = 0x21;        // Draw line
static const uint8_t SSD1331_DRAWRECT = 0x22;        // Draw rectangle
static const uint8_t SSD1331_FILL = 0x26;            // Fill enable/disable
static const uint8_t SSD1331_SETCOLUMN = 0x15;       // Set column address
static const uint8_t SSD1331_SETROW = 0x75;          // Set row address
static const uint8_t SSD1331_CONTRASTA = 0x81;       // Set contrast for color A
static const uint8_t SSD1331_CONTRASTB = 0x82;       // Set contrast for color B
static const uint8_t SSD1331_CONTRASTC = 0x83;       // Set contrast for color C
static const uint8_t SSD1331_MASTERCURRENT = 0x87;   // Master current control
static const uint8_t SSD1331_SETREMAP = 0xA0;        // Set re-map & data format
static const uint8_t SSD1331_STARTLINE = 0xA1;       // Set display start line
static const uint8_t SSD1331_DISPLAYOFFSET = 0xA2;   // Set display offset
static const uint8_t SSD1331_NORMALDISPLAY = 0xA4;   // Set display to normal mode
static const uint8_t SSD1331_DISPLAYALLON = 0xA5;    // Set entire display ON
static const uint8_t SSD1331_DISPLAYALLOFF = 0xA6;   // Set entire display OFF
static const uint8_t SSD1331_INVERTDISPLAY = 0xA7;   // Invert display
static const uint8_t SSD1331_SETMULTIPLEX = 0xA8;    // Set multiplex ratio
static const uint8_t SSD1331_SETMASTER = 0xAD;       // Set master configuration
static const uint8_t SSD1331_DISPLAYOFF = 0xAE;      // Display OFF (sleep mode)
static const uint8_t SSD1331_DISPLAYON = 0xAF;       // Normal Brightness Display ON
static const uint8_t SSD1331_POWERMODE = 0xB0;       // Power save mode
static const uint8_t SSD1331_PRECHARGE = 0xB1;       // Phase 1 and 2 period adjustment
static const uint8_t SSD1331_CLOCKDIV = 0xB3;        // Set display clock divide ratio/oscillator frequency
static const uint8_t SSD1331_PRECHARGEA = 0x8A;      // Set second pre-charge speed for color A
static const uint8_t SSD1331_PRECHARGEB = 0x8B;      // Set second pre-charge speed for color B
static const uint8_t SSD1331_PRECHARGEC = 0x8C;      // Set second pre-charge speed for color C
static const uint8_t SSD1331_PRECHARGELEVEL = 0xBB;  // Set pre-charge voltage
static const uint8_t SSD1331_VCOMH = 0xBE;           // Set Vcomh voltge

void SSD1331::setup() {
  this->init_internal_(this->get_buffer_length_());

  this->command(SSD1331_DISPLAYOFF);  // 0xAE
  this->command(SSD1331_SETREMAP);    // 0xA0
  this->command(0x72);                // RGB Color
  this->command(SSD1331_STARTLINE);   // 0xA1
  this->command(0x0);
  this->command(SSD1331_DISPLAYOFFSET);  // 0xA2
  this->command(0x0);
  this->command(SSD1331_NORMALDISPLAY);  // 0xA4
  this->command(SSD1331_SETMULTIPLEX);   // 0xA8
  this->command(0x3F);                   // 0x3F 1/64 duty
  this->command(SSD1331_SETMASTER);      // 0xAD
  this->command(0x8E);
  this->command(SSD1331_POWERMODE);  // 0xB0
  this->command(0x0B);
  this->command(SSD1331_PRECHARGE);  // 0xB1
  this->command(0x31);
  this->command(SSD1331_CLOCKDIV);    // 0xB3
  this->command(0xF0);                // 7:4 = Oscillator Frequency, 3:0 = CLK Div Ratio, (A[3:0]+1 = 1..16)
  this->command(SSD1331_PRECHARGEA);  // 0x8A
  this->command(0x64);
  this->command(SSD1331_PRECHARGEB);  // 0x8B
  this->command(0x78);
  this->command(SSD1331_PRECHARGEC);  // 0x8C
  this->command(0x64);
  this->command(SSD1331_PRECHARGELEVEL);  // 0xBB
  this->command(0x3A);
  this->command(SSD1331_VCOMH);  // 0xBE
  this->command(0x3E);
  this->command(SSD1331_MASTERCURRENT);  // 0x87
  this->command(0x06);
  set_brightness(this->brightness_);
  this->fill(Color::BLACK);  // clear display - ensures we do not see garbage at power-on
  this->display();           // ...write buffer, which actually clears the display's memory
  this->turn_on();           // display ON
}
void SSD1331::display() {
  this->command(SSD1331_SETCOLUMN);  // set column address
  this->command(0x00);               // set column start address
  this->command(0x5F);               // set column end address
  this->command(SSD1331_SETROW);     // set row address
  this->command(0x00);               // set row start address
  this->command(0x3F);               // set last row
  this->write_display_data();
}
void SSD1331::update() {
  this->do_update_();
  this->display();
}
void SSD1331::set_brightness(float brightness) {
  // validation
  this->brightness_ = clamp(brightness, 0.0F, 1.0F);
  // now write the new brightness level to the display
  this->command(SSD1331_CONTRASTA);  // 0x81
  this->command(int(SSD1331_MAX_CONTRASTA * (this->brightness_)));
  this->command(SSD1331_CONTRASTB);  // 0x82
  this->command(int(SSD1331_MAX_CONTRASTB * (this->brightness_)));
  this->command(SSD1331_CONTRASTC);  // 0x83
  this->command(int(SSD1331_MAX_CONTRASTC * (this->brightness_)));
}
bool SSD1331::is_on() { return this->is_on_; }
void SSD1331::turn_on() {
  this->command(SSD1331_DISPLAYON);
  this->is_on_ = true;
}
void SSD1331::turn_off() {
  this->command(SSD1331_DISPLAYOFF);
  this->is_on_ = false;
}
int SSD1331::get_height_internal() { return 64; }
int SSD1331::get_width_internal() { return 96; }
size_t SSD1331::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) * size_t(SSD1331_BYTESPERPIXEL);
}
void HOT SSD1331::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;
  const uint32_t color565 = display::ColorUtil::color_to_565(color);
  // where should the bits go in the big buffer array? math...
  uint16_t pos = (x + y * this->get_width_internal()) * SSD1331_BYTESPERPIXEL;
  this->buffer_[pos++] = (color565 >> 8) & 0xff;
  this->buffer_[pos] = color565 & 0xff;
}
void SSD1331::fill(Color color) {
  const uint32_t color565 = display::ColorUtil::color_to_565(color);
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++) {
    if (i & 1) {
      this->buffer_[i] = color565 & 0xff;
    } else {
      this->buffer_[i] = (color565 >> 8) & 0xff;
    }
  }
}
void SSD1331::init_reset_() {
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

}  // namespace ssd1331_base
}  // namespace esphome
