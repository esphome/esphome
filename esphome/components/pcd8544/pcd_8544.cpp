#include "pcd_8544.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace pcd8544 {

static const char *const TAG = "pcd_8544";

void PCD8544::setup_pins_() {
  this->spi_setup();
  this->init_reset_();
  this->dc_pin_->setup();
}

void PCD8544::init_reset_() {
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

void PCD8544::initialize() {
  this->init_internal_(this->get_buffer_length_());

  this->command(this->PCD8544_FUNCTIONSET | this->PCD8544_EXTENDEDINSTRUCTION);
  // LCD bias select (4 is optimal?)
  this->command(this->PCD8544_SETBIAS | 0x04);

  // contrast
  this->command(this->PCD8544_SETVOP | this->contrast_);

  // normal mode
  this->command(this->PCD8544_FUNCTIONSET);

  // Set display to Normal
  this->command(this->PCD8544_DISPLAYCONTROL | this->PCD8544_DISPLAYNORMAL);
}

void PCD8544::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}
void PCD8544::end_command_() { this->disable(); }
void PCD8544::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void PCD8544::end_data_() { this->disable(); }

int PCD8544::get_width_internal() { return 84; }
int PCD8544::get_height_internal() { return 48; }

size_t PCD8544::get_buffer_length_() {
  return size_t(this->get_width_internal()) * size_t(this->get_height_internal()) / 8u;
}

void HOT PCD8544::display() {
  uint8_t col, maxcol, p;

  for (p = 0; p < 6; p++) {
    this->command(this->PCD8544_SETYADDR | p);

    // start at the beginning of the row
    col = 0;
    maxcol = this->get_width_internal() - 1;

    this->command(this->PCD8544_SETXADDR | col);

    this->start_data_();
    for (; col <= maxcol; col++) {
      this->write_byte(this->buffer_[(this->get_width_internal() * p) + col]);
    }
    this->end_data_();
  }

  this->command(this->PCD8544_SETYADDR);
}

void HOT PCD8544::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0) {
    return;
  }

  uint16_t pos = x + (y / 8) * this->get_width_internal();
  uint8_t subpos = y % 8;
  if (color.is_on()) {
    this->buffer_[pos] |= (1 << subpos);
  } else {
    this->buffer_[pos] &= ~(1 << subpos);
  }
}

void PCD8544::dump_config() {
  LOG_DISPLAY("", "PCD8544", this);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void PCD8544::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}

void PCD8544::update() {
  this->do_update_();
  this->display();
}

void PCD8544::fill(Color color) {
  uint8_t fill = color.is_on() ? 0xFF : 0x00;
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->buffer_[i] = fill;
}

}  // namespace pcd8544
}  // namespace esphome
