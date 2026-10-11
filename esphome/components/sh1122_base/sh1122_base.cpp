#include "sh1122_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::sh1122_base {

ESPHOME_LOG_TAG(TAG, "sh1122");

static constexpr uint8_t SH1122_MAX_CONTRAST = 255;
static constexpr uint8_t SH1122_COLORMASK = 0x0f;
static constexpr uint8_t SH1122_COLORSHIFT = 4;
static constexpr uint8_t SH1122_PIXELSPERBYTE = 2;

static constexpr uint8_t SH1122_SETLOWCOLUMNADDRESS = 0x00;
static constexpr uint8_t SH1122_SETHIGHCOLUMNADDRESS = 0x10;
static constexpr uint8_t SH1122_SETDISCHARGEVSLLEVEL = 0x30;
static constexpr uint8_t SH1122_SETDISPLAYSTARTLINE = 0x40;
static constexpr uint8_t SH1122_SETCONTRASTCURRENT = 0x81;
static constexpr uint8_t SH1122_SETSEGMENTREMAP = 0xA0;
static constexpr uint8_t SH1122_SETDISPLAYOFFON = 0xA4;
static constexpr uint8_t SH1122_SETNORMALDISPLAY = 0xA6;
static constexpr uint8_t SH1122_SETMULTIPLEXRATIO = 0xA8;
static constexpr uint8_t SH1122_DCDCSETTING = 0xAD;
static constexpr uint8_t SH1122_SETDISPLAYOFF = 0xAE;
static constexpr uint8_t SH1122_SETDISPLAYON = 0xAF;
static constexpr uint8_t SH1122_SETROWADDRESS = 0xB0;
static constexpr uint8_t SH1122_SETSCANDIRECTION = 0xC0;
static constexpr uint8_t SH1122_SETDISPLAYOFFSET = 0xD3;
static constexpr uint8_t SH1122_SETCLOCKDIVIDER = 0xD5;
static constexpr uint8_t SH1122_SETDISCHARGEPRECHARGEPERIOD = 0xD9;
static constexpr uint8_t SH1122_SETVCOMDESELECTLEVEL = 0xDB;
static constexpr uint8_t SH1122_SETVSEGMLEVEL = 0xDC;

void SH1122::setup() {
  this->init_internal_(this->get_buffer_length_());
  if (this->buffer_ == nullptr) {
    this->mark_failed();
    return;
  }

  this->turn_off();
  this->command2_(SH1122_SETCLOCKDIVIDER, 0x50);
  this->command2_(SH1122_SETMULTIPLEXRATIO, 0x3F);
  this->command2_(SH1122_SETDISPLAYOFFSET, 0x00);
  this->command2_(SH1122_SETROWADDRESS, 0x00);
  this->command_(SH1122_SETDISPLAYSTARTLINE | 32);  // the panel's rows start half way down the 128 line RAM
  this->command_(SH1122_SETDISCHARGEVSLLEVEL);
  this->command2_(SH1122_DCDCSETTING, 0x81);
  this->command_(SH1122_SETSEGMENTREMAP | 0x01);   // column 0 on the right
  this->command_(SH1122_SETSCANDIRECTION | 0x08);  // rows scanned bottom up
  this->command2_(SH1122_SETDISCHARGEPRECHARGEPERIOD, 0x28);
  this->command2_(SH1122_SETVCOMDESELECTLEVEL, 0x35);
  this->command2_(SH1122_SETVSEGMLEVEL, 0x35);
  this->command_(SH1122_SETNORMALDISPLAY);
  this->command_(SH1122_SETHIGHCOLUMNADDRESS);
  this->command_(SH1122_SETLOWCOLUMNADDRESS);
  this->command_(SH1122_SETDISPLAYOFFON);
  this->set_brightness(this->brightness_);
  this->display();  // the buffer starts cleared; push it so no power-on garbage shows
  this->turn_on();
}
void SH1122::display() {
  // Return the RAM pointer to the top left corner in one transfer, then stream the frame
  static constexpr uint8_t SET_ADDRESS[] = {SH1122_SETHIGHCOLUMNADDRESS, SH1122_SETLOWCOLUMNADDRESS,
                                            SH1122_SETROWADDRESS, 0x00};
  this->write_command(SET_ADDRESS, sizeof(SET_ADDRESS));
  this->write_display_data();
}
void SH1122::update() {
  this->do_update_();
  this->display();
}
void SH1122::set_brightness(float brightness) {
  this->brightness_ = clamp(brightness, 0.0F, 1.0F);
  this->command2_(SH1122_SETCONTRASTCURRENT, int(SH1122_MAX_CONTRAST * (this->brightness_)));
}
void SH1122::turn_on() {
  this->command_(SH1122_SETDISPLAYON);
  this->is_on_ = true;
}
void SH1122::turn_off() {
  this->command_(SH1122_SETDISPLAYOFF);
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
  const int width = this->get_width_internal();
  if (x >= width || x < 0 || y >= this->get_height_internal() || y < 0)
    return;
  // Two pixels per byte, the left one in the high nibble
  const uint32_t color4 = display::ColorUtil::color_to_grayscale4(color) & SH1122_COLORMASK;
  const uint16_t pos = (x / SH1122_PIXELSPERBYTE) + (y * width / SH1122_PIXELSPERBYTE);
  const uint8_t shift = (1u - (x % SH1122_PIXELSPERBYTE)) * SH1122_COLORSHIFT;
  this->buffer_[pos] = (this->buffer_[pos] & (static_cast<uint8_t>(~SH1122_COLORMASK) >> shift)) | (color4 << shift);
}
void SH1122::fill(Color color) {
  // If clipping is active, fall back to base implementation
  if (this->get_clipping().is_set()) {
    Display::fill(color);
    return;
  }
  const uint32_t color4 = display::ColorUtil::color_to_grayscale4(color);
  const uint8_t fill = (color4 & SH1122_COLORMASK) | ((color4 & SH1122_COLORMASK) << SH1122_COLORSHIFT);
  memset(this->buffer_, fill, this->get_buffer_length_());
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

}  // namespace esphome::sh1122_base
