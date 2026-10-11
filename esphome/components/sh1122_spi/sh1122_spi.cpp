#include "sh1122_spi.h"
#include "esphome/core/log.h"

namespace esphome::sh1122_spi {

ESPHOME_LOG_TAG(TAG, "sh1122_spi");

void SPISH1122::setup() {
  this->spi_setup();
  this->dc_pin_->setup();

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
// enable() and disable() drive the CS pin
void SPISH1122::write_command_(const uint8_t *bytes, size_t len) {
  this->dc_pin_->digital_write(false);
  delay(1);
  this->enable();
  this->write_array(bytes, len);
  this->disable();
}
void HOT SPISH1122::write_display_data() {
  this->dc_pin_->digital_write(true);
  delay(1);
  this->enable();
  this->write_array(this->buffer_, this->get_buffer_length_());
  this->disable();
}

}  // namespace esphome::sh1122_spi
