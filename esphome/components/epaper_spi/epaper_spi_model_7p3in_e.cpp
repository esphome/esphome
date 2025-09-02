#include "epaper_spi_model_7p3in_e.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.7.3in-e";

void EPaper7p3InE::power_on() {
  ESP_LOGI(TAG, "Power on the display");
  this->command(0x04);
  this->waiting_for_idle_ = true;
}

void EPaper7p3InE::power_off() {
  ESP_LOGI(TAG, "Power off the display");
  this->command(0x02);
  this->data(0x00);
  this->waiting_for_idle_ = true;
}

void EPaper7p3InE::refresh_screen() {
  ESP_LOGI(TAG, "Refresh the display");
  this->command(0x12);
  this->data(0x00);
  this->waiting_for_idle_ = true;
}

void EPaper7p3InE::deep_sleep() {
  ESP_LOGI(TAG, "Set the display to deep sleep");
  this->command(0x07);
  this->data(0xA5);
}

void EPaper7p3InE::dump_config() {
  LOG_DISPLAY("", "E-Paper SPI", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.3in-E");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::epaper_spi
