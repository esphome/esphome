#include "ssd1306_base.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ssd1306_base {

static const char *const TAG = "ssd1306";

static const uint8_t SSD1306_MAX_CONTRAST = 255;
static const uint8_t SSD1305_MAX_BRIGHTNESS = 255;

static const uint8_t SSD1306_COMMAND_DISPLAY_OFF = 0xAE;
static const uint8_t SSD1306_COMMAND_DISPLAY_ON = 0xAF;
static const uint8_t SSD1306_COMMAND_SET_DISPLAY_CLOCK_DIV = 0xD5;
static const uint8_t SSD1306_COMMAND_SET_MULTIPLEX = 0xA8;
static const uint8_t SSD1306_COMMAND_SET_DISPLAY_OFFSET_Y = 0xD3;
static const uint8_t SSD1306_COMMAND_SET_START_LINE = 0x40;
static const uint8_t SSD1306_COMMAND_CHARGE_PUMP = 0x8D;
static const uint8_t SSD1306_COMMAND_MEMORY_MODE = 0x20;
static const uint8_t SSD1306_COMMAND_SEGRE_MAP = 0xA0;
static const uint8_t SSD1306_COMMAND_COM_SCAN_INC = 0xC0;
static const uint8_t SSD1306_COMMAND_COM_SCAN_DEC = 0xC8;
static const uint8_t SSD1306_COMMAND_SET_COM_PINS = 0xDA;
static const uint8_t SSD1306_COMMAND_SET_CONTRAST = 0x81;
static const uint8_t SSD1306_COMMAND_SET_PRE_CHARGE = 0xD9;
static const uint8_t SSD1306_COMMAND_SET_VCOM_DETECT = 0xDB;
static const uint8_t SSD1306_COMMAND_DISPLAY_ALL_ON_RESUME = 0xA4;
static const uint8_t SSD1306_COMMAND_DEACTIVATE_SCROLL = 0x2E;
static const uint8_t SSD1306_COMMAND_COLUMN_ADDRESS = 0x21;
static const uint8_t SSD1306_COMMAND_PAGE_ADDRESS = 0x22;
static const uint8_t SSD1306_COMMAND_NORMAL_DISPLAY = 0xA6;
static const uint8_t SSD1306_COMMAND_INVERSE_DISPLAY = 0xA7;

static const uint8_t SSD1305_COMMAND_SET_BRIGHTNESS = 0x82;
static const uint8_t SSD1305_COMMAND_SET_AREA_COLOR = 0xD8;

static const uint8_t SH1107_COMMAND_SET_START_LINE = 0xDC;
static const uint8_t SH1107_COMMAND_CHARGE_PUMP = 0xAD;

void SSD1306::setup() {
  this->init_internal_(this->get_buffer_length_());

  // SH1107 resources
  //
  // Datasheet v2.3:
  // www.displayfuture.com/Display/datasheet/controller/SH1107.pdf
  // Adafruit C++ driver:
  // github.com/adafruit/Adafruit_SH110x
  // Adafruit CircuitPython driver:
  // github.com/adafruit/Adafruit_CircuitPython_DisplayIO_SH1107

  // Turn off display during initialization (0xAE)
  this->command(SSD1306_COMMAND_DISPLAY_OFF);

  // If SH1107, use POR defaults (0x50) = divider 1, frequency +0%
  if (!this->is_sh1107_()) {
    // Set oscillator frequency to 4'b1000 with no clock division (0xD5)
    this->command(SSD1306_COMMAND_SET_DISPLAY_CLOCK_DIV);
    // Oscillator frequency <= 4'b1000, no clock division
    this->command(0x80);
  }

  // Enable low power display mode for SSD1305 (0xD8)
  if (this->is_ssd1305_()) {
    this->command(SSD1305_COMMAND_SET_AREA_COLOR);
    this->command(0x05);
  }

  // Set mux ratio to [Y pixels - 1] (0xA8)
  this->command(SSD1306_COMMAND_SET_MULTIPLEX);
  this->command(this->get_height_internal() - 1);

  // Set Y offset (0xD3)
  this->command(SSD1306_COMMAND_SET_DISPLAY_OFFSET_Y);
  this->command(0x00 + this->offset_y_);

  if (this->is_sh1107_()) {
    // Set start line at line 0 (0xDC)
    this->command(SH1107_COMMAND_SET_START_LINE);
    this->command(0x00);
  } else {
    // Set start line at line 0 (0x40)
    this->command(SSD1306_COMMAND_SET_START_LINE | 0x00);
  }

  if (this->is_ssd1305_()) {
    // SSD1305 does not have charge pump
  } else if (this->is_sh1107_()) {
    // Enable charge pump (0xAD)
    this->command(SH1107_COMMAND_CHARGE_PUMP);
    if (this->external_vcc_) {
      this->command(0x8A);
    } else {
      this->command(0x8B);
    }
  } else {
    // Enable charge pump (0x8D)
    this->command(SSD1306_COMMAND_CHARGE_PUMP);
    if (this->external_vcc_) {
      this->command(0x10);
    } else {
      this->command(0x14);
    }
  }

  // Set addressing mode to horizontal (0x20)
  this->command(SSD1306_COMMAND_MEMORY_MODE);
  if (!this->is_sh1107_()) {
    // SH1107 memory mode is a 1 byte command
    this->command(0x00);
  }
  // X flip mode (0xA0, 0xA1)
  this->command(SSD1306_COMMAND_SEGRE_MAP | this->flip_x_);

  // Y flip mode (0xC0, 0xC8)
  this->command(SSD1306_COMMAND_COM_SCAN_INC | (this->flip_y_ << 3));

  if (!this->is_sh1107_()) {
    // Set pin configuration (0xDA)
    this->command(SSD1306_COMMAND_SET_COM_PINS);
    switch (this->model_) {
      case SSD1306_MODEL_128_32:
      case SH1106_MODEL_128_32:
      case SSD1306_MODEL_96_16:
      case SH1106_MODEL_96_16:
        this->command(0x02);
        break;
      case SSD1306_MODEL_128_64:
      case SH1106_MODEL_128_64:
      case SSD1306_MODEL_64_48:
      case SSD1306_MODEL_64_32:
      case SH1106_MODEL_64_48:
      case SSD1305_MODEL_128_32:
      case SSD1305_MODEL_128_64:
      case SSD1306_MODEL_72_40:
        this->command(0x12);
        break;
      case SH1107_MODEL_128_64:
      case SH1107_MODEL_128_128:
        // Not used, but prevents build warning
        break;
    }
  }

  // Pre-charge period (0xD9)
  this->command(SSD1306_COMMAND_SET_PRE_CHARGE);
  if (this->external_vcc_) {
    this->command(0x22);
  } else {
    this->command(0xF1);
  }

  // Set V_COM (0xDB)
  this->command(SSD1306_COMMAND_SET_VCOM_DETECT);
  switch (this->model_) {
    case SH1106_MODEL_128_64:
    case SH1107_MODEL_128_64:
    case SH1107_MODEL_128_128:
      this->command(0x35);
      break;
    case SSD1306_MODEL_72_40:
      this->command(0x20);
      break;
    default:
      this->command(0x00);
      break;
  }

  // Display output follow RAM (0xA4)
  this->command(SSD1306_COMMAND_DISPLAY_ALL_ON_RESUME);

  // Inverse display mode (0xA6, 0xA7)
  this->set_invert(this->invert_);

  // Disable scrolling mode (0x2E)
  this->command(SSD1306_COMMAND_DEACTIVATE_SCROLL);

  // Contrast and brighrness
  // SSD1306 does not have brightness setting
  set_contrast(this->contrast_);
  if (this->is_ssd1305_())
    set_brightness(this->brightness_);

  this->fill(Color::BLACK);  // clear display - ensures we do not see garbage at power-on
  this->display();           // ...write buffer, which actually clears the display's memory

  this->turn_on();
}
void SSD1306::display() {
  if (this->is_sh1106_() || this->is_sh1107_()) {
    this->write_display_data();
    return;
  }

  this->command(SSD1306_COMMAND_COLUMN_ADDRESS);
  switch (this->model_) {
    case SSD1306_MODEL_64_48:
    case SSD1306_MODEL_64_32:
      this->command(0x20 + this->offset_x_);
      this->command(0x20 + this->offset_x_ + this->get_width_internal() - 1);
      break;
    case SSD1306_MODEL_72_40:
      this->command(0x1C + this->offset_x_);
      this->command(0x1C + this->offset_x_ + this->get_width_internal() - 1);
      break;
    default:
      this->command(0 + this->offset_x_);  // Page start address, 0
      this->command(this->get_width_internal() + this->offset_x_ - 1);
      break;
  }

  this->command(SSD1306_COMMAND_PAGE_ADDRESS);
  // Page start address, 0
  this->command(0);
  // Page end address:
  this->command((this->get_height_internal() / 8) - 1);

  this->write_display_data();
}
bool SSD1306::is_sh1106_() const {
  return this->model_ == SH1106_MODEL_96_16 || this->model_ == SH1106_MODEL_128_32 ||
         this->model_ == SH1106_MODEL_128_64;
}
bool SSD1306::is_sh1107_() const { return this->model_ == SH1107_MODEL_128_64 || this->model_ == SH1107_MODEL_128_128; }
bool SSD1306::is_ssd1305_() const {
  return this->model_ == SSD1305_MODEL_128_64 || this->model_ == SSD1305_MODEL_128_64;
}
void SSD1306::update() {
  this->do_update_();
  this->display();
}

void SSD1306::set_invert(bool invert) {
  this->invert_ = invert;
  // Inverse display mode (0xA6, 0xA7)
  this->command(SSD1306_COMMAND_NORMAL_DISPLAY | this->invert_);
}
void SSD1306::set_contrast(float contrast) {
  // validation
  this->contrast_ = clamp(contrast, 0.0F, 1.0F);
  // now write the new contrast level to the display (0x81)
  this->command(SSD1306_COMMAND_SET_CONTRAST);
  this->command(int(SSD1306_MAX_CONTRAST * (this->contrast_)));
}
void SSD1306::set_brightness(float brightness) {
  // validation
  if (!this->is_ssd1305_())
    return;
  this->brightness_ = clamp(brightness, 0.0F, 1.0F);
  // now write the new brightness level to the display (0x82)
  this->command(SSD1305_COMMAND_SET_BRIGHTNESS);
  this->command(int(SSD1305_MAX_BRIGHTNESS * (this->brightness_)));
}
bool SSD1306::is_on() { return this->is_on_; }
void SSD1306::turn_on() {
  this->command(SSD1306_COMMAND_DISPLAY_ON);
  this->is_on_ = true;
}
void SSD1306::turn_off() {
  this->command(SSD1306_COMMAND_DISPLAY_OFF);
  this->is_on_ = false;
}
int SSD1306::get_height_internal() {
  switch (this->model_) {
    case SH1107_MODEL_128_64:
    case SH1107_MODEL_128_128:
      return 128;
    case SSD1306_MODEL_128_32:
    case SSD1306_MODEL_64_32:
    case SH1106_MODEL_128_32:
    case SSD1305_MODEL_128_32:
      return 32;
    case SSD1306_MODEL_128_64:
    case SH1106_MODEL_128_64:
    case SSD1305_MODEL_128_64:
      return 64;
    case SSD1306_MODEL_96_16:
    case SH1106_MODEL_96_16:
      return 16;
    case SSD1306_MODEL_64_48:
    case SH1106_MODEL_64_48:
      return 48;
    case SSD1306_MODEL_72_40:
      return 40;
    default:
      return 0;
  }
}
int SSD1306::get_width_internal() {
  switch (this->model_) {
    case SSD1306_MODEL_128_32:
    case SH1106_MODEL_128_32:
    case SSD1306_MODEL_128_64:
    case SH1106_MODEL_128_64:
    case SSD1305_MODEL_128_32:
    case SSD1305_MODEL_128_64:
    case SH1107_MODEL_128_128:
      return 128;
    case SSD1306_MODEL_96_16:
    case SH1106_MODEL_96_16:
      return 96;
    case SSD1306_MODEL_64_48:
    case SSD1306_MODEL_64_32:
    case SH1106_MODEL_64_48:
    case SH1107_MODEL_128_64:
      return 64;
    case SSD1306_MODEL_72_40:
      return 72;
    default:
      return 0;
  }
}
size_t SSD1306::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
}
void HOT SSD1306::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;

  uint16_t pos = x + (y / 8) * this->get_width_internal();
  uint8_t subpos = y & 0x07;
  if (color.is_on()) {
    this->buffer_[pos] |= (1 << subpos);
  } else {
    this->buffer_[pos] &= ~(1 << subpos);
  }
}
void SSD1306::fill(Color color) {
  uint8_t fill = color.is_on() ? 0xFF : 0x00;
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}
void SSD1306::init_reset_() {
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
const char *SSD1306::model_str_() {
  switch (this->model_) {
    case SSD1306_MODEL_128_32:
      return "SSD1306 128x32";
    case SSD1306_MODEL_128_64:
      return "SSD1306 128x64";
    case SSD1306_MODEL_64_32:
      return "SSD1306 64x32";
    case SSD1306_MODEL_96_16:
      return "SSD1306 96x16";
    case SSD1306_MODEL_64_48:
      return "SSD1306 64x48";
    case SSD1306_MODEL_72_40:
      return "SSD1306 72x40";
    case SH1106_MODEL_128_32:
      return "SH1106 128x32";
    case SH1106_MODEL_128_64:
      return "SH1106 128x64";
    case SH1106_MODEL_96_16:
      return "SH1106 96x16";
    case SH1106_MODEL_64_48:
      return "SH1106 64x48";
    case SH1107_MODEL_128_64:
      return "SH1107 128x64";
    case SSD1305_MODEL_128_32:
      return "SSD1305 128x32";
    case SSD1305_MODEL_128_64:
      return "SSD1305 128x64";
    default:
      return "Unknown";
  }
}

}  // namespace ssd1306_base
}  // namespace esphome
