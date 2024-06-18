#include "st7567_base.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace st7567_base {

static const char *const TAG = "st7567";

void ST7567::setup() {
  this->init_model_();

  this->init_internal_(this->get_buffer_length_());
  this->init_reset_();
  this->reset_();
  this->display_init_();
}

void ST7567::init_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(1);
  }
}

void ST7567::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(20);
    this->reset_pin_->digital_write(true);
    delay(20);
  }
}

void ST7567::display_init_() {
  ESP_LOGD(TAG, "Initializing ST7567 display...");
  this->display_init_registers_();
  this->clear();
  this->write_display_data();
  this->command(ST7567_DISPLAY_ON);
}

void ST7567::display_init_registers_() { this->device_config_.display_init(); }

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

int ST7567::get_width_internal() { return this->device_config_.memory_width; }

int ST7567::get_height_internal() { return this->device_config_.memory_height; }

int ST7567::get_offset_x_() { return mirror_x_ ? device_config_.memory_width - device_config_.visible_width : 0; };

size_t ST7567::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
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

void ST7567::init_model_() {
  // common init
  auto common_init = [this]() {
    this->command(ST7567_BIAS_9);
    this->command(this->mirror_x_ ? ST7567_SEG_REVERSE : ST7567_SEG_NORMAL);
    this->command(this->mirror_y_ ? ST7567_COM_NORMAL : ST7567_COM_REMAP);

    this->command(ST7567_BOOSTER_ON);
    delay(10);
    this->command(ST7567_REGULATOR_ON);
    delay(10);
    this->command(ST7567_POWER_ON);
    delay(10);

    // this->set_brightness(this->brightness_);
    // this->set_contrast(this->contrast_);

    this->command(ST7567_INVERT_OFF | this->invert_colors_);
    this->command(ST7567_PIXELS_NORMAL | this->all_pixels_on_);
    this->command(ST7567_SCAN_START_LINE);
  };

  switch (this->device_config_.model) {
    case ST7567Model::ST7567_128x64:
      this->device_config_.name = "ST7567";
      this->device_config_.visible_width = 128;
      this->device_config_.visible_height = 64;  // 65 actually, 1 bit line for icons, not supported
      this->device_config_.memory_width = 132;
      this->device_config_.memory_height = 64;
      this->device_config_.display_init = [this, common_init]() {
        common_init();
        this->set_brightness(this->brightness_);
        this->set_contrast(this->contrast_);
      };
      break;
    case ST7567Model::ST7570_128x128:
      this->device_config_.name = "ST7570";
      this->device_config_.memory_width = 128;
      this->device_config_.memory_height = 128;  // 129 actually, 1 bit line for icons, not supported
      this->device_config_.visible_width = 128;
      this->device_config_.visible_height = 128;
      this->device_config_.display_init = [this, common_init]() { common_init(); };
      break;

    case ST7567Model::ST7570_102x102a:
      this->device_config_.name = "ST7570a";
      this->device_config_.memory_width = 128;
      this->device_config_.memory_height = 128;
      this->device_config_.visible_width = 102;
      this->device_config_.visible_height = 102;
      this->device_config_.display_init = [this, common_init]() { common_init(); };
      break;

    case ST7567Model::ST7570_102x102b:
      this->device_config_.name = "ST7570b";
      this->device_config_.memory_width = 104;
      this->device_config_.memory_height = 104;
      this->device_config_.visible_width = 102;
      this->device_config_.visible_height = 102;
      this->device_config_.display_init = [this, common_init]() { common_init(); };
      break;

    default:
      this->mark_failed();
  }
}

std::string ST7567::model_str_() {
  return str_sprintf("%s (%dx%d)", this->device_config_.name, this->device_config_.visible_width,
                     this->device_config_.visible_height);
}

}  // namespace st7567_base
}  // namespace esphome
