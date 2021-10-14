#include "ili9486_display.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ili9486 {

static const char *const TAG = "ili9486";

void ILI9486Display::setup_pins_() {
  this->init_internal_multiple_(this->get_buffer_length_());
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
  }
  if (this->led_pin_ != nullptr) {
    this->led_pin_->setup();
    this->led_pin_->digital_write(true);
  }
  this->spi_setup();

  this->reset_();
}

void ILI9486Display::dump_config() {
  LOG_DISPLAY("", "ili9486", this);
  ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d,  Rotation: %d", this->width_, this->height_, this->rotation_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  Backlight Pin: ", this->led_pin_);
  LOG_UPDATE_INTERVAL(this);
}

float ILI9486Display::get_setup_priority() const { return setup_priority::PROCESSOR; }
void ILI9486Display::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}

void ILI9486Display::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void ILI9486Display::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

void ILI9486Display::send_command(uint8_t command_byte, const uint8_t *data_bytes, uint8_t num_data_bytes) {
  this->command(command_byte);  // Send the command byte
  this->start_data_();
  this->write_array(data_bytes, num_data_bytes);
  this->end_data_();
}

uint8_t ILI9486Display::read_command(uint8_t command_byte, uint8_t index) {
  uint8_t data = 0x10 + index;
  this->send_command(0xD9, &data, 1);  // Set Index Register
  uint8_t result;
  this->start_command_();
  this->write_byte(command_byte);
  this->start_data_();
  do {
    result = this->read_byte();
  } while (index--);
  this->end_data_();
  return result;
}

void ILI9486Display::update() {
  this->do_update_();
  this->display_();
}

void ILI9486Display::display_() {
  // we will only update the changed window to the display
  int w = this->x_high_ - this->x_low_ + 1;
  int h = this->y_high_ - this->y_low_ + 1;

  set_addr_window_(this->x_low_, this->y_low_, w, h);
  this->start_data_();
  uint32_t start_pos = ((this->y_low_ * this->width_) + x_low_);
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      uint32_t pos = start_pos + (row * width_) + col;

      uint16_t color = convert_to_16bit_color_(buffer_multiple_[display_buffer_part_(pos)][display_buffer_pos_(pos)]);
      this->write_byte(color >> 8);
      this->write_byte(color);
    }
  }
  this->end_data_();

  // invalidate watermarks
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

uint16_t ILI9486Display::convert_to_16bit_color_(uint8_t color_8bit) {
  int r = color_8bit >> 5;
  int g = (color_8bit >> 2) & 0x07;
  int b = color_8bit & 0x03;
  uint16_t color = (r * 0x04) << 11;
  color |= (g * 0x09) << 5;
  color |= (b * 0x0A);

  return color;
}

uint8_t ILI9486Display::convert_to_8bit_color_(uint16_t color_16bit) {
  // convert 16bit color to 8 bit buffer
  uint8_t r = color_16bit >> 11;
  uint8_t g = (color_16bit >> 5) & 0x3F;
  uint8_t b = color_16bit & 0x1F;

  return ((b / 0x0A) | ((g / 0x09) << 2) | ((r / 0x04) << 5));
}

uint8_t ILI9486Display::convert_color_to_8bit_color_(Color color) {
  // convert 32bit color to 8 bit buffer
  uint8_t r = color.r >> 3;  // 5
  uint8_t g = color.g >> 2;  // 6
  uint8_t b = color.b >> 3;  // 5

  return ((b / 0x0A) | ((g / 0x09) << 2) | ((r / 0x04) << 5));
}

uint16_t ILI9486Display::convert_color_to_16bit_color_(Color color) {
  // convert 32bit color to 8 bit buffer
  uint8_t r = color.r >> 3;  // 5
  uint8_t g = color.g >> 2;  // 6
  uint8_t b = color.b >> 3;  // 5

  return (b | (g << 5) | (r << 11));
}

void ILI9486Display::fill(Color color) {
  for (auto &i : this->buffer_multiple_) {
    memset(i, convert_color_to_8bit_color_(color), this->get_buffer_length_() / DISPLAY_BUFFER_PARTS);
  }
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->get_width_internal() - 1;
  this->y_high_ = this->get_height_internal() - 1;
}

void ILI9486Display::fill_internal_(Color color) {
  this->set_addr_window_(0, 0, this->get_width_internal(), this->get_height_internal());
  this->start_data_();

  uint16_t color565 = convert_color_to_16bit_color_(color);
  for (uint32_t i = 0; i < (this->get_width_internal()) * (this->get_height_internal()); i++) {
    this->write_byte(color565 >> 8);
    this->write_byte(color565);
    buffer_multiple_[display_buffer_part_(i)][display_buffer_pos_(i)] = 0;
  }
  this->end_data_();
}

void HOT ILI9486Display::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;

  // low and high watermark may speed up drawing from buffer
  this->x_low_ = (x < this->x_low_) ? x : this->x_low_;
  this->y_low_ = (y < this->y_low_) ? y : this->y_low_;
  this->x_high_ = (x > this->x_high_) ? x : this->x_high_;
  this->y_high_ = (y > this->y_high_) ? y : this->y_high_;

  uint32_t pos = (y * width_) + x;
  buffer_multiple_[display_buffer_part_(pos)][display_buffer_pos_(pos)] = convert_color_to_8bit_color_(color);
}

// should return the total size: return this->get_width_internal() * this->get_height_internal() * 2 // 16bit color
// values per bit is huge
uint32_t ILI9486Display::get_buffer_length_() { return this->get_width_internal() * this->get_height_internal(); }

void ILI9486Display::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}

void ILI9486Display::end_command_() { this->disable(); }
void ILI9486Display::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void ILI9486Display::end_data_() { this->disable(); }

void ILI9486Display::init_lcd_(const uint8_t *init_cmd) {
  uint8_t cmd, x, num_args;
  const uint8_t *addr = init_cmd;
  while ((cmd = progmem_read_byte(addr++)) > 0) {
    x = progmem_read_byte(addr++);
    num_args = x & 0x7F;
    send_command(cmd, addr, num_args);
    addr += num_args;
    if (x & 0x80)
      delay(150);  // NOLINT
  }
}

void ILI9486Display::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h) {
  uint16_t x2 = (x1 + w - 1), y2 = (y1 + h - 1);
  this->command(ILI9486_CASET);  // Column address set
  this->start_data_();
  this->write_byte(x1 >> 8);
  this->write_byte(x1);
  this->write_byte(x2 >> 8);
  this->write_byte(x2);
  this->end_data_();
  this->command(ILI9486_PASET);  // Row address set
  this->start_data_();
  this->write_byte(y1 >> 8);
  this->write_byte(y1);
  this->write_byte(y2 >> 8);
  this->write_byte(y2);
  this->end_data_();
  this->command(ILI9486_RAMWR);  // Write to RAM
}

void ILI9486Display::invert_display_(bool invert) { this->command(invert ? ILI9486_INVON : ILI9486_INVOFF); }

int ILI9486Display::get_width_internal() { return this->width_; }
int ILI9486Display::get_height_internal() { return this->height_; }

//   24_TFT display
void ILI9486TFT35::initialize() {
  this->init_lcd_(INITCMD_TFT35);
  this->width_ = 320;
  this->height_ = 480;
  this->fill_internal_(COLOR_BLACK);
}

}  // namespace ili9486
}  // namespace esphome
