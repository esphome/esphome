#include "epaper_spi_model_7p3in_spectra_e6.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.7.3in-spectra-e6";

void EPaper7p3InSpectraE6::power_on() {
  ESP_LOGI(TAG, "Power on");
  this->command(0x04);
  this->waiting_for_idle_ = true;
}

void EPaper7p3InSpectraE6::power_off() {
  ESP_LOGI(TAG, "Power off");
  this->command(0x02);
  this->data(0x00);
  this->waiting_for_idle_ = true;
}

void EPaper7p3InSpectraE6::refresh_screen() {
  ESP_LOGI(TAG, "Refresh");
  this->command(0x12);
  this->data(0x00);
  this->waiting_for_idle_ = true;
}

void EPaper7p3InSpectraE6::deep_sleep() {
  ESP_LOGI(TAG, "Deep sleep");
  this->command(0x07);
  this->data(0xA5);
}

void EPaper7p3InSpectraE6::dump_config() {
  LOG_DISPLAY("", "E-Paper SPI", this);
  ESP_LOGCONFIG(TAG, "  Model: 7.3in Spectra E6");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::epaper_spi
