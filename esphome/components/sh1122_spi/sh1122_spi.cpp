#include "sh1122_spi.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace sh1122_spi {

static const char *const TAG = "sh1122_spi";

void SPISH1122::setup() {
  this->spi_setup();
  this->dc_pin_->setup();  // OUTPUT
  if (this->cs_)
    this->cs_->setup();  // OUTPUT

  this->init_reset_();
  delay(500);  // NOLINT
  SH1122::setup();
}
void SPISH1122::dump_config() {
  LOG_DISPLAY("", "SPI SH1122", this);
  ESP_LOGCONFIG(TAG,
                "  Model: %s\n"
                "  Initial Brightness: %.2f",
                this->model_str_(), this->brightness_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_UPDATE_INTERVAL(this);
}
void SPISH1122::command(uint8_t value) {
  if (this->cs_)
    this->cs_->digital_write(true);
  this->dc_pin_->digital_write(false);
  delay(1);
  this->enable();
  if (this->cs_)
    this->cs_->digital_write(false);
  this->write_byte(value);
  if (this->cs_)
    this->cs_->digital_write(true);
  this->disable();
}
void SPISH1122::command2(uint8_t value, uint8_t data) {
  if (this->cs_)
    this->cs_->digital_write(true);
  this->dc_pin_->digital_write(false);
  delay(1);
  this->enable();
  if (this->cs_)
    this->cs_->digital_write(false);
  this->write_byte(value);
  this->write_byte(data);
  if (this->cs_)
    this->cs_->digital_write(true);
  this->disable();
}
void SPISH1122::data(uint8_t value) {
  if (this->cs_)
    this->cs_->digital_write(true);
  this->dc_pin_->digital_write(true);
  delay(1);
  this->enable();
  if (this->cs_)
    this->cs_->digital_write(false);
  this->write_byte(value);
  if (this->cs_)
    this->cs_->digital_write(true);
  this->disable();
}
void HOT SPISH1122::write_display_data() {
  if (this->cs_)
    this->cs_->digital_write(true);
  this->dc_pin_->digital_write(true);
  if (this->cs_)
    this->cs_->digital_write(false);
  delay(1);
  this->enable();
  this->write_array(this->buffer_, this->get_buffer_length_());
  if (this->cs_)
    this->cs_->digital_write(true);
  this->disable();
}

}  // namespace sh1122_spi
}  // namespace esphome
