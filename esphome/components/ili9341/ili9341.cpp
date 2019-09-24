#include "ili9341.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ili9341 {

static const char *TAG = "ili9341";

void ili9341::setup_pins_() {
  this->init_internal_(this->get_buffer_length_());
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

void ili9341::dump_config() {
  LOG_DISPLAY("", "ili9341", this);
  ESP_LOGCONFIG(TAG, "  Width: %d, Height: %d,  Rotation: %d", this->width_, this->height_, this->rotation_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  Backlight Pin: ", this->led_pin_);
  LOG_UPDATE_INTERVAL(this);
}

float ili9341::get_setup_priority() const { return setup_priority::PROCESSOR; }
void ili9341::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}

void ili9341::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(200);
    this->reset_pin_->digital_write(true);
    delay(200);
  }
}

void ili9341::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

void ili9341::send_command(uint8_t commandByte, const uint8_t *dataBytes, uint8_t numDataBytes) {
  this->command(commandByte);  // Send the command byte
  this->start_data_();
  for (int i = 0; i < numDataBytes; i++) {
    this->write_byte(*dataBytes);  // Send the data bytes
    dataBytes++;
  }
  this->end_data_();
}

uint8_t ili9341::read_command_(uint8_t commandByte, uint8_t index) {
  uint8_t data = 0x10 + index;
  this->send_command(0xD9, &data, 1);  // Set Index Register
  uint8_t result;
  this->start_command_();
  this->write_byte(commandByte);
  this->start_data_();
  do {
    result = this->read_byte();
  } while (index--);
  this->end_data_();
  return result;
}

void ili9341::update() {
  this->do_update_();
  this->display();
}

void ili9341::display () {
  //we will only update the changed window to the display
  int w = this->x_high_ - this->x_low_;
  int h = this->y_high_ - this->y_low_;

  set_addr_window_(this->x_low_,this->y_low_,w, h);
  this->start_data_();
  uint32_t start_pos = ((this->y_low_ * this->width_) + x_low_);
  for (uint16_t row = 0; row < h; row++) {
    for(uint16_t col = 0; col < w; col++) {
      uint32_t pos = start_pos + (row * width_) + col;

      uint16_t color = convert_to_16bit_color_(buffer_[pos]);
      this->write_byte(color >> 8);
      this->write_byte(color);  
    }
  }
  this->end_data_();

  //invalidate watermarks
  this->x_low_ = this->width_;
  this->y_low_ = this->height_;
  this->x_high_ = 0;
  this->y_high_ = 0;
}

uint16_t ili9341::convert_to_16bit_color_ (uint8_t color_8bit) {
    int r = color_8bit >> 5;
    int g = (color_8bit >> 2 )& 0x07;
    int b = color_8bit & 0x03;
    uint16_t color = (r * 0x04) << 11;
    color |= (g * 0x09) << 5;
    color |= (b * 0x0A);
    
    return color;
}

uint8_t ili9341::convert_to_8bit_color (uint16_t color_16bit) {
  //convert 16bit color to 8 bit buffer
  uint8_t r = color_16bit >> 11;
  uint8_t g = (color_16bit >> 5 ) &  0x3F;
  uint8_t b = color_16bit & 0x1F;

  return((b / 0x0A) | ((g / 0x09) << 2) | ((r / 0x04) << 5));
}

/**
 * do nothing.
 * we need this function het to override the default behaviour.
 * Otherwise the buffer is cleared at every update
 * */
void ili9341::fill(int color) {
  //do nothing.
}

void ili9341::fill_internal_(int color) {
  this->set_addr_window_(0, 0, this->get_width_internal(), this->get_height_internal());
  this->start_data_();
  for (uint32_t i = 0; i < (this->get_width_internal()) * (this->get_height_internal()); i++) {
    this->write_byte(color >> 8);
    this->write_byte(color);
    buffer_[i] = 0;
  }
  this->end_data_();
}

void HOT ili9341::draw_absolute_pixel_internal(int x, int y, int color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0)
    return;

  // low and high watermark may speed up drawing from buffer
  this->x_low_ = (x < this->x_low_) ? x : this->x_low_;
  this->y_low_ = (y < this->y_low_) ? y : this->y_low_;  
  this->x_high_ = (x > this->x_high_) ? x : this->x_high_;
  this->y_high_ = (y > this->y_high_) ? y : this->y_high_;

  uint32_t pos = (y*width_) + x;
  buffer_[pos] = convert_to_8bit_color(color);
}

// should return the total size: return this->get_width_internal() * this->get_height_internal() * 2 // 16bit color
// values per bit is huge
uint32_t ili9341::get_buffer_length_() { return this->get_width_internal() * this->get_height_internal(); }

void ili9341::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}

void ili9341::end_command_() { this->disable(); }
void ili9341::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void ili9341::end_data_() { this->disable(); }

void ili9341::init_lcd_(const uint8_t *init_cmd) {
  uint8_t cmd, x, numArgs;
  const uint8_t *addr = init_cmd;
  while ((cmd = pgm_read_byte(addr++)) > 0) {
    x = pgm_read_byte(addr++);
    numArgs = x & 0x7F;
    send_command(cmd, addr, numArgs);
    addr += numArgs;
    if (x & 0x80)
      delay(150);
  }
}

void ili9341::set_addr_window_(uint16_t x1, uint16_t y1, uint16_t w, uint16_t h) {
  uint16_t x2 = (x1 + w - 1), y2 = (y1 + h - 1);
  this->command(ILI9341_CASET);  // Column address set
  this->start_data_();
  this->write_byte(x1 >> 8);
  this->write_byte(x1);
  this->write_byte(x2 >> 8);
  this->write_byte(x2);
  this->end_data_();
  this->command(ILI9341_PASET);  // Row address set
  this->start_data_();
  this->write_byte(y1 >> 8);
  this->write_byte(y1);
  this->write_byte(y2 >> 8);
  this->write_byte(y2);
  this->end_data_();
  this->command(ILI9341_RAMWR);  // Write to RAM
}

void ili9341::invert_display_(bool invert) { this->command(invert ? ILI9341_INVON : ILI9341_INVOFF); }

int ili9341::get_width_internal() { return this->width_; }
int ili9341::get_height_internal() { return this->height_; }

//   M5Stack display
void ili9341_M5Stack::initialize() {
  this->init_lcd_(initcmd_m5stack);
  this->width_ = 320;
  this->height_ = 240;
  this->fill_internal_(BLACK);
}

//   24_TFT display
void ili9341_24_TFT::initialize() {
  this->init_lcd_(initcmd_tft);
  this->width_ = 240;
  this->height_ = 320;
  this->fill_internal_(BLACK);
}

}  // namespace ili9341
}  // namespace esphome
