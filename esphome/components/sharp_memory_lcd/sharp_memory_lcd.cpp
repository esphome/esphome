#include "sharp_memory_lcd.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace sharp_memory_lcd {

static const uint8_t SHARPMEM_BIT_WRITECMD = 0x01;  // 0x80 in LSB format
static const uint8_t SHARPMEM_BIT_VCOM = 0x02;      // 0x40 in LSB format
static const uint8_t SHARPMEM_BIT_CLEAR = 0x04;     // 0x20 in LSB format

static const char *const TAG = "sharp_memory_lcd";

#define TOGGLE_VCOM \
  do { \
    _sharpmem_vcom = _sharpmem_vcom ? 0x00 : SHARPMEM_BIT_VCOM; \
  } while (0);

void SharpMemoryLCD::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SharpMemoryLCD...");
  this->dump_config();
  this->spi_setup();
  this->init_internal_(this->get_buffer_length_());

  if (this->extmode_) {
    this->extmode_->digital_write(false);
  }
  if (this->extcomin_) {
    this->extcomin_->digital_write(false);
  }
  if (this->disp_) {
    this->disp_->digital_write(false);
  }

  display_init_();
}

void HOT SharpMemoryLCD::write_display_data() {
  ESP_LOGD(TAG, "Writing display data...");

  uint16_t i, currentline;

  this->enable();

  this->transfer_byte(this->sharpmem_vcom_ | SHARPMEM_BIT_WRITECMD);
  this->sharpmem_vcom_ = this->sharpmem_vcom_ ? 0x00 : SHARPMEM_BIT_VCOM;

  uint16_t width = this->get_width_internal();
  uint16_t height = this->get_height_internal();

  uint8_t bytes_per_line = width / 8;

  uint16_t totalbytes = (width * height) / 8;

  for (i = 0; i < totalbytes; i += bytes_per_line) {
    uint8_t line[bytes_per_line + 2];

    // Send address _byte
    currentline = ((i + 1) / (width / 8)) + 1;
    line[0] = currentline;
    // Copy over this line
    memcpy(line + 1, this->buffer_ + i, bytes_per_line);
    // Attach end of line and send it
    line[bytes_per_line + 1] = 0x00;
    this->transfer_array(line, bytes_per_line + 2);
    App.feed_wdt();
  }
  // Send another trailing 8 bits for the last line
  this->transfer_byte(0x00);
  this->disable();
}

void SharpMemoryLCD::fill(Color color) {
  uint8_t fill = color.is_on() ? 0x00 : 0xFF;
  if (this->invert_color_) {
    fill = ~fill;
  }
  memset(this->buffer_, fill, this->get_buffer_length_());
}

void SharpMemoryLCD::dump_config() {
  LOG_DISPLAY("", "SharpMemoryLCD", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  EXTMODE Pin: ", this->extmode_);
  LOG_PIN("  EXTCOMIN Pin: ", this->extcomin_);
  LOG_PIN("  DISP Pin: ", this->disp_);
  ESP_LOGCONFIG(TAG, "  Height: %d", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %d", this->width_);
  ESP_LOGCONFIG(TAG, "  Inverted colors: %s", TRUEFALSE(this->invert_color_));
}

void SharpMemoryLCD::update() {
  ESP_LOGD(TAG, "Updating display...");
  this->clear();
  if (this->writer_local_.has_value())  // call lambda function if available
    (*this->writer_local_)(*this);
  this->write_display_data();
}

int SharpMemoryLCD::get_width_internal() { return this->width_; }

int SharpMemoryLCD::get_height_internal() { return this->height_; }

size_t SharpMemoryLCD::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
}

void HOT SharpMemoryLCD::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    return;
  }
  int width = this->get_width_internal() / 8u;

  if (this->invert_color_ == color.is_on) {
    this->buffer_[y * width + x / 8] |= (0x01 << (x & 7));
  } else {
    this->buffer_[y * width + x / 8] &= ~(0x01 << (x & 7));
  }
}

void SharpMemoryLCD::display_init_() {
  ESP_LOGD(TAG, "Initializing display...");
  // Set the vcom bit to a defined state
  this->sharpmem_vcom_ = SHARPMEM_BIT_VCOM;
  this->write_display_data();
}

}  // namespace sharp_memory_lcd
}  // namespace esphome
