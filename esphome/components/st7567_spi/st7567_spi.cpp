#include "st7567_spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st7567_spi {

static const char *const TAG = "st7567_spi";

void SPIST7567::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPI ST7567 display...");

  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);

  this->spi_setup();

  this->init_reset_();
  ST7567::setup();
}

void SPIST7567::dump_config() {
  LOG_DISPLAY("", "SPI ST7567", this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_str_().c_str());
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  D/C Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", YESNO(this->mirror_x_));
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", YESNO(this->mirror_y_));
  ESP_LOGCONFIG(TAG, "  Invert Colors: %s", YESNO(this->invert_colors_));
  LOG_UPDATE_INTERVAL(this);
}

void SPIST7567::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}

void SPIST7567::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}
void SPIST7567::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}

void SPIST7567::end_command_() { this->disable(); }

void SPIST7567::end_data_() { this->disable(); }

void HOT SPIST7567::write_display_data() {
  // ST7567A has built-in RAM with 132x65 bit capacity which stores the display data.
  // but only first 128 pixels from each line are shown on screen
  // if screen got flipped horizontally then it shows last 128 pixels,
  // so we need to write x coordinate starting from column 4, not column 0

  this->command(esphome::st7567_base::ST7567_SET_START_LINE + this->start_line_);

  for (uint8_t y = 0; y < (uint8_t) this->get_height_internal() / 8; y++) {
    this->command(esphome::st7567_base::ST7567_PAGE_ADDR + y);                       // Set Page
    this->command(esphome::st7567_base::ST7567_COL_ADDR_H);                          // Set MSB Column address
    this->command(esphome::st7567_base::ST7567_COL_ADDR_L + this->get_offset_x_());  // Set LSB Column address

    this->start_data_();
    this->write_array(&this->buffer_[y * this->get_width_internal()], this->get_width_internal());
    this->end_data_();
  }
}

}  // namespace st7567_spi
}  // namespace esphome
