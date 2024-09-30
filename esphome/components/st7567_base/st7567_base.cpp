#include "st7567_base.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace st7567_base {

static const char *const TAG = "st7567";

void ST7567::setup() {
  ESP_LOGD(TAG, "Setting up component");
  this->init_model_();

  this->init_internal_(this->get_framebuffer_size_());  // display buffer

  this->setup_reset_pin_();
  this->reset_lcd_hw_();
  this->setup_lcd_();
}

void ST7567::update() {
  this->do_update_();
  // As per datasheet: It is recommended to use the refresh sequence regularly in a specified interval.
  if (this->refresh_requested_) {
    this->refresh_requested_ = false;
    this->perform_display_refresh_();
  }
  auto now = millis();
  this->write_display_data_();
  ESP_LOGV(TAG, "Data write took %dms", (unsigned) (millis() - now));
}

void ST7567::dump_config() {
  LOG_DISPLAY("", "ST7567", this);

  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_().c_str());
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", YESNO(this->mirror_x_));
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", YESNO(this->mirror_y_));
  ESP_LOGCONFIG(TAG, "  Invert Colors: %s", YESNO(this->invert_colors_));
  ESP_LOGCONFIG(TAG, "  Contrast: %u", this->contrast_);
  LOG_UPDATE_INTERVAL(this);

  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

void ST7567::fill(Color color) { memset(buffer_, color.is_on() ? 0xFF : 0x00, this->get_framebuffer_size_()); }

void ST7567::request_refresh() {
  // as per datasheet: It is recommended to use the refresh sequence regularly in a specified interval.
  this->refresh_requested_ = true;
}

void ST7567::turn_on() { this->command_(ST7567_DISPLAY_ON); }

void ST7567::turn_off() { this->command_(ST7567_DISPLAY_OFF); }

void ST7567::change_contrast(uint8_t val) {
  this->set_contrast(val);
  this->command_(ST7567_SET_EV_CMD);
  this->command_(this->contrast_);
}

void ST7567::change_brightness(uint8_t val) {
  this->set_brightness(val);
  this->command_(ST7567_RESISTOR_RATIO | this->brightness_);
}

void ST7567::invert_colors(bool invert_colors) {
  this->set_invert_colors(invert_colors);
  this->command_(ST7567_INVERT_OFF | this->invert_colors_);
}

void ST7567::enable_all_pixels_on(bool enable) { this->command_(ST7567_PIXELS_NORMAL | enable); }

void ST7567::set_scroll(uint8_t line) { this->start_line_ = line % this->get_height_internal(); }

/* Private part */

void ST7567::setup_reset_pin_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(false);  // keep low until reset
    delay(1);
  }
}

void ST7567::init_model_() {
  auto st7567_init = [this]() {
    this->command_(ST7567_BIAS_9);

    // mirror Y by default, to make (0,0) be top left
    this->command_(this->mirror_y_ ? ST7567_COM_NORMAL : ST7567_COM_REMAP);
    this->command_(this->mirror_x_ ? ST7567_SEG_REVERSE : ST7567_SEG_NORMAL);

    this->change_brightness(this->brightness_);
    this->command_(ST7567_SET_EV_CMD);
    this->command_(this->contrast_);

    this->command_(ST7567_INVERT_OFF | this->invert_colors_);
    this->command_(ST7567_PIXELS_NORMAL | this->all_pixels_on_);
    this->command_(ST7567_SCAN_START_LINE);

    this->command_(ST7567_BOOSTER_ON);               // Power Control, VC: ON VR: OFF VF: OFF
    delay(this->device_config_.wait_between_power);  // For ST7570 Minimum Delay 100ms!
    this->command_(ST7567_REGULATOR_ON);             // Power Control, VC: ON VR: ON VF: OFF
    delay(this->device_config_.wait_between_power);  // For ST7570 Minimum Delay 100ms!
    this->command_(ST7567_POWER_ON);                 // Power Control, VC: ON VR: ON VF: ON
    delay(5);
  };

  auto st7570_init = [this]() {
    this->command_(0x7B);
    this->command_(0x11);
    this->command_(0x00);

    // Initial internal counters
    this->command_(0x25);             // Initial power counter
    this->command_(ST7570_MODE_SET);  // MODE SET
    this->command_(0x08);             // FR=0000 => 77Hz; // BE[1:0]=1,0 => BE Level-3

    // mirror Y by default, to make (0,0) be top left
    this->command_(this->mirror_y_ ? ST7567_COM_REMAP : ST7567_COM_NORMAL);
    this->command_(this->mirror_x_ ? ST7567_SEG_REVERSE : ST7567_SEG_NORMAL);

    this->command_(0x44);  // Set initial COM0 register
    this->command_(0x00);  //
    this->command_(0x40);  // Set display start line register
    this->command_(0x00);  //
    this->command_(0x4C);  // Set N-line Inversion
    this->command_(0x00);  //

    this->command_(ST7570_OSCILLATOR_ON);  // OSC ON

    this->command_(ST7567_SET_EV_CMD);
    this->command_(this->contrast_);
    this->command_(0x81);                            // Set Contrast
    this->command_(53);                              // EV=xx
    this->command_(ST7567_BOOSTER_ON);               // Power Control, VC: ON VR: OFF VF: OFF
    delay(this->device_config_.wait_between_power);  // For ST7570 Minimum Delay 100ms!
    this->command_(ST7567_REGULATOR_ON);             // Power Control, VC: ON VR: ON VF: OFF
    delay(this->device_config_.wait_between_power);  // For ST7570 Minimum Delay 100ms!
    this->command_(ST7567_POWER_ON);                 // Power Control, VC: ON VR: ON VF: ON
    delay(20);
  };

  auto st7567_set_start_line = [this](uint8_t x) {
    this->command_(ST7567_SET_START_LINE + x);  // one-byte command
  };
  auto st7570_set_start_line = [this](uint8_t x) {
    this->command_(ST7567_SET_START_LINE);  // two-byte command
    this->command_(x);
  };

  switch (this->model_) {
    case ST7567Model::ST7567_128x64:
      this->device_config_.name = "ST7567";
      this->device_config_.visible_width = 128;
      this->device_config_.visible_height = 64;
      this->device_config_.memory_width = 132;
      this->device_config_.memory_height = 64;
      this->device_config_.visible_offset_x_mirror = 4;
      this->device_config_.init_procedure = st7567_init;
      this->device_config_.command_set_start_line = st7567_set_start_line;
      break;

    case ST7567Model::ST7570_128x128:
      this->device_config_.name = "ST7570";
      this->device_config_.memory_width = 128;
      this->device_config_.memory_height = 128;
      this->device_config_.visible_width = 128;
      this->device_config_.visible_height = 128;
      this->device_config_.init_procedure = st7570_init;
      this->device_config_.command_set_start_line = st7570_set_start_line;
      this->device_config_.wait_between_power = 100;
      break;

    case ST7567Model::ST7570_102x102:
      this->device_config_.name = "ST7570";
      this->device_config_.memory_width = 128;
      this->device_config_.memory_height = 128;
      this->device_config_.visible_width = 102;
      this->device_config_.visible_height = 102;
      this->device_config_.visible_offset_x_normal = 0;
      this->device_config_.visible_offset_x_mirror = 26;
      this->device_config_.visible_offset_y_normal = 0;
      this->device_config_.visible_offset_y_mirror = 26;
      this->device_config_.init_procedure = st7570_init;
      this->device_config_.command_set_start_line = st7570_set_start_line;
      this->device_config_.wait_between_power = 100;
      break;

    default:
      this->mark_failed();
  }

  // to avoid getting out of memory bounds check width/x.
  // vis_w + off_x shall be <= max_x !!!
  assert(this->device_config_.visible_width + this->device_config_.visible_offset_x_mirror <=
         this->device_config_.memory_width);
  assert(this->device_config_.visible_width + this->device_config_.visible_offset_x_mirror <=
         this->device_config_.memory_width);
}

void ST7567::setup_lcd_() {
  this->perform_lcd_init_();

  // Init DDRAM garbage
  this->clear();
  this->write_display_data_();

  // Turn on display clear and nice
  this->turn_on();
}

void ST7567::reset_lcd_hw_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(20);
    this->reset_pin_->digital_write(true);
    delay(120);
  }
}

void ST7567::reset_lcd_sw_() {
  const uint8_t ST7567_SOFT_RESET = 0b11101000;
  this->command_(ST7567_SOFT_RESET);
}

void ST7567::perform_lcd_init_() { this->device_config_.init_procedure(); }

void ST7567::perform_display_refresh_() {
  ESP_LOGD(TAG, "Performing refresh sequence...");
  this->command_(ST7567_SW_REFRESH);
  this->perform_lcd_init_();
}

int ST7567::get_width_internal() { return this->device_config_.visible_width; }

int ST7567::get_height_internal() { return this->device_config_.visible_height; }

uint8_t ST7567::get_visible_area_offset_x_() {
  return this->mirror_x_ ? device_config_.visible_offset_x_mirror : device_config_.visible_offset_x_normal;
};

uint8_t ST7567::get_visible_area_offset_y_() {
  return this->mirror_y_ ? device_config_.visible_offset_y_mirror : device_config_.visible_offset_y_normal;
};

size_t ST7567::get_framebuffer_size_() {
  return size_t(this->device_config_.memory_width) * size_t(this->device_config_.memory_height) / 8u;
}

void ST7567::command_set_start_line_() {
  uint8_t start_line = this->start_line_ + this->get_visible_area_offset_y_();
  start_line %= this->device_config_.memory_height;

  this->device_config_.command_set_start_line(start_line);
}

void HOT ST7567::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }

  x += this->get_visible_area_offset_x_();
  y += this->get_visible_area_offset_y_();

  uint16_t pos = x + (y / 8) * this->device_config_.memory_width;
  uint8_t subpos = y & 0x07;
  if (color.is_on()) {
    this->buffer_[pos] |= (1 << subpos);
  } else {
    this->buffer_[pos] &= ~(1 << subpos);
  }
}

std::string ST7567::model_str_() {
  return str_sprintf("%s (%dx%d)", this->device_config_.name, this->device_config_.visible_width,
                     this->device_config_.visible_height);
}

}  // namespace st7567_base
}  // namespace esphome
