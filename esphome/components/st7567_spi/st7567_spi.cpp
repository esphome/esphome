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

  ST7567::setup();
}

void SPIST7567::dump_config() {
  ST7567::dump_config();
  ESP_LOGCONFIG(TAG, "  Interface: 4-wire SPI");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  D/C Pin: ", this->dc_pin_);
}

void SPIST7567::command_(uint8_t value) {
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

void HOT SPIST7567::write_display_data_() {
  for (uint8_t page = 0; page < this->device_config_.memory_height / 8; page++) {
    this->command_(esphome::st7567_base::ST7567_PAGE_ADDR + page);  // Set Page
    this->command_(esphome::st7567_base::ST7567_COL_ADDR_H);        // Set MSB Column address
    this->command_(esphome::st7567_base::ST7567_COL_ADDR_L);        // Set LSB Column address

    this->start_data_();
    this->write_array(&this->buffer_[page * this->device_config_.memory_width], this->device_config_.memory_width);
    this->end_data_();
  }
}

}  // namespace st7567_spi
}  // namespace esphome
