#include "sharpMem.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace sharpMem {

static const char *const TAG = "sharpMem";

#define TOGGLE_VCOM                                                            \
  do {                                                                         \
    _sharpmem_vcom = _sharpmem_vcom ? 0x00 : SHARPMEM_BIT_VCOM;                \
  } while (0);

void SharpMem::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SharpMem...");
  this->dump_config();
  this->spi_setup();
  this->init_internal_(this->get_buffer_length_());
  this->cs_->digital_write(false);
  this->extmode_->digital_write(false);
  this->extcomin_->digital_write(false);
  this->disp_->digital_write(false);

  display_init_();
}

void HOT SharpMem::write_display_data() {
  ESP_LOGD(TAG, "Writing display data...");

  uint16_t i, currentline;

  this->enable();
  // Send the write command, this display has active HIGH
  this->cs_->digital_write(true);

  this->transfer_byte(_sharpmem_vcom | SHARPMEM_BIT_WRITECMD);
  TOGGLE_VCOM;

  uint16_t width = this->get_width_internal();
  uint16_t height = this->get_height_internal();

  uint8_t bytes_per_line = width / 8;

  uint16_t totalbytes = (width * height) / 8;

  for (i = 0; i < totalbytes; i += bytes_per_line) {
    uint8_t line[bytes_per_line + 2];

    // Send address byte
    currentline = ((i + 1) / (width / 8)) + 1;
    line[0] = currentline;
    // Copy over this line
    memcpy(line + 1, this->buffer_ + i, bytes_per_line);
    // Reorder bits on every byte of the line (MSB -> LSB)
    // Invert byte to paint black on white
    for(uint8_t j = 0; j < bytes_per_line; j++){
      uint8_t currentByte = line[j+1];
      //line[j+1] = ((currentByte * 0x0802LU & 0x22110LU) | (currentByte * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
      if(!this->invert_color_){
        line[j+1] = ~line[j+1];
      }
    }
    // Attach end of line and send it
    line[bytes_per_line + 1] = 0x00;
    this->transfer_array(line, bytes_per_line + 2);
    App.feed_wdt();
  }

  // Send another trailing 8 bits for the last line
  this->transfer_byte(0x00);
  this->cs_->digital_write(false);
  this->disable();
}

void SharpMem::fill(Color color) { memset(this->buffer_, color.is_on() ? 0xFF : 0x00, this->get_buffer_length_()); }

void SharpMem::dump_config() {
  LOG_DISPLAY("", "SharpMem", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  EXTMODE Pin: ", this->extmode_);
  LOG_PIN("  EXTCOMIN Pin: ", this->extcomin_);
  LOG_PIN("  DISP Pin: ", this->disp_);
  ESP_LOGCONFIG(TAG, "  Height: %d", this->height_);
  ESP_LOGCONFIG(TAG, "  Width: %d", this->width_);
  ESP_LOGCONFIG(TAG, "  Inverted colors: %s", this->invert_color_ ? "true" : "false");
}

void SharpMem::update() {
  ESP_LOGD(TAG, "Updating display...");
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
    this->buffer_[y * width + x / 8] |= (0x01 >> (x & 7));
  } else {
    this->buffer_[y * width + x / 8] &= ~(0x01 >> (x & 7));
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
