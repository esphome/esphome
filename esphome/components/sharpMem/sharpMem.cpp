#include "sharpMem.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace sharpMem {

static const char *const TAG = "sharpMem";

void SharpMem::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SharpMem...");
  this->dump_config();
  this->spi_setup();
  this->init_internal_(this->get_buffer_length_());
  display_init_();
}

void HOT SharpMem::write_display_data() {
  uint16_t i, currentline;

  this->enable();
  // Send the write command
  digitalWrite(this->cs_, HIGH);

  this->transfer_byte(_sharpmem_vcom | SHARPMEM_BIT_WRITECMD); // eventually transfer_array
  TOGGLE_VCOM;

  uint8_t width = this->get_width_internal();
  uint8_t height = this->get_height_internal();
  uint8_t bytes_per_line = width / 8;
  uint16_t totalbytes = (width * height) / 8;

  for (i = 0; i < totalbytes; i += bytes_per_line) {
    uint8_t line[bytes_per_line + 2];

    // Send address byte
    currentline = ((i + 1) / (width / 8)) + 1;
    line[0] = currentline;
    // copy over this line
    memcpy(line + 1, this->buffer_ + i, bytes_per_line);
    // Send end of line
    line[bytes_per_line + 1] = 0x00;
    // send it!
    this->transfer_array(line, bytes_per_line + 2);
    App.feed_wdt();
  }

  // Send another trailing 8 bits for the last line
  this->transfer_byte(0x00);
  digitalWrite(this->cs_, LOW);
  this->disable();
}

void SharpMem::fill(Color color) { memset(this->buffer_, color.is_on() ? 0xFF : 0x00, this->get_buffer_length_()); }

void SharpMem::dump_config() {
  LOG_DISPLAY("", "SharpMem", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Height: %d", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %d", this->width_);
}

void SharpMem::update() {
  this->clear();
  if (this->writer_local_.has_value())  // call lambda function if available
    (*this->writer_local_)(*this);
  this->write_display_data();
}

int SharpMem::get_width_internal() { return this->width_; }

int SharpMem::get_height_internal() { return this->height_; }

size_t SharpMem::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
}

void HOT SharpMem::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || x < 0 || y >= this->get_height_internal() || y < 0) {
    ESP_LOGW(TAG, "Position out of area: %dx%d", x, y);
    return;
  }
  int width = this->get_width_internal() / 8u;
  if (color.is_on()) {
    this->buffer_[y * width + x / 8] |= (0x80 >> (x & 7));
  } else {
    this->buffer_[y * width + x / 8] &= ~(0x80 >> (x & 7));
  }
}

void SharpMem::display_init_() {
  ESP_LOGD(TAG, "Initializing display...");
  // Set the vcom bit to a defined state
  _sharpmem_vcom = SHARPMEM_BIT_VCOM;
  this->write_display_data();
}

}  // namespace sharpMem
}  // namespace esphome
