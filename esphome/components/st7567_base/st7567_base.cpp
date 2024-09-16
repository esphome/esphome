#include "st7567_base.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace st7567_base {

static const char *const TAG = "st7567";

void ST7567::setup() {
  this->init_internal_(this->get_buffer_length_());
  this->display_init_();
}

void ST7567::display_init_() {
  ESP_LOGD(TAG, "Initializing ST7567 display...");
  this->display_init_registers_();
  this->clear();
  this->write_display_data();
  this->command(ST7567_DISPLAY_ON);
}

void ST7567::display_init_registers_() {
  this->command(ST7567_BIAS_9);
  this->command(this->mirror_x_ ? ST7567_SEG_REVERSE : ST7567_SEG_NORMAL);
  this->command(this->mirror_y_ ? ST7567_COM_NORMAL : ST7567_COM_REMAP);
  this->command(ST7567_POWER_CTL | 0x4);
  this->command(ST7567_POWER_CTL | 0x6);
  this->command(ST7567_POWER_CTL | 0x7);

  this->set_brightness(this->brightness_);
  this->set_contrast(this->contrast_);

  this->command(ST7567_INVERT_OFF | this->invert_colors_);

  this->command(ST7567_BOOSTER_ON);
  this->command(ST7567_REGULATOR_ON);
  this->command(ST7567_POWER_ON);

  this->command(ST7567_SCAN_START_LINE);
  this->command(ST7567_PIXELS_NORMAL | this->all_pixels_on_);
}

void ST7567::display_sw_refresh_() {
  ESP_LOGD(TAG, "Performing refresh sequence...");
  this->command(ST7567_SW_REFRESH);
  this->display_init_registers_();
}

void ST7567::request_refresh() {
  // as per datasheet: It is recommended to use the refresh sequence regularly in a specified interval.
  this->refresh_requested_ = true;
}

void ST7567::update() {
  this->do_update_();
  if (this->refresh_requested_) {
    this->refresh_requested_ = false;
    this->display_sw_refresh_();
  }
  this->write_display_data();
}

void ST7567::set_all_pixels_on(bool enable) {
  this->all_pixels_on_ = enable;
  this->command(ST7567_PIXELS_NORMAL | this->all_pixels_on_);
}

void ST7567::set_invert_colors(bool invert_colors) {
  this->invert_colors_ = invert_colors;
  this->command(ST7567_INVERT_OFF | this->invert_colors_);
}

void ST7567::set_contrast(uint8_t val) {
  this->contrast_ = val & 0b111111;
  // 0..63, 26 is normal

  // two byte command
  // first byte 0x81
  // second byte 0-63

  this->command(ST7567_SET_EV_CMD);
  this->command(this->contrast_);
}

void ST7567::set_brightness(uint8_t val) {
  this->brightness_ = val & 0b111;
  // 0..7, 5 normal

  //********Adjust display brightness********
  // 0x20-0x27 is the internal Rb/Ra resistance
  // adjustment setting of V5 voltage RR=4.5V

  this->command(ST7567_RESISTOR_RATIO | this->brightness_);
}

bool ST7567::is_on() { return this->is_on_; }

void ST7567::turn_on() {
  this->command(ST7567_DISPLAY_ON);
  this->is_on_ = true;
}

void ST7567::turn_off() {
  this->command(ST7567_DISPLAY_OFF);
  this->is_on_ = false;
}

void ST7567::set_scroll(uint8_t line) { this->start_line_ = line % this->get_height_internal(); }

int ST7567::get_width_internal() { return 128; }

int ST7567::get_height_internal() { return 64; }

// 128x64, but memory size 132x64, line starts from 0, but if mirrored then it starts from 131, not 127
size_t ST7567::get_buffer_length_() {
  return size_t(this->get_width_internal() + 4) * size_t(this->get_height_internal()) / 8u;
}

void HOT ST7567::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }

  uint16_t pos = x + (y / 8) * this->get_width_internal();
  uint8_t subpos = y & 0x07;
  if (color.is_on()) {
    this->buffer_[pos] |= (1 << subpos);
  } else {
    this->buffer_[pos] &= ~(1 << subpos);
  }
}

void ST7567::fill(Color color) { memset(buffer_, color.is_on() ? 0xFF : 0x00, this->get_buffer_length_()); }

void ST7567::init_reset_() {
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

const char *ST7567::model_str_() { return "ST7567 128x64"; }

}  // namespace st7567_base
}  // namespace esphome
