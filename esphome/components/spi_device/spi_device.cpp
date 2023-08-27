#include "spi_device.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace spi_device {

static const char *const TAG = "spi_device";

void SPIDeviceComponent::setup() {
  ESP_LOGD(TAG, "Setting up SPIDevice...");
  this->spi_setup();
  ESP_LOGCONFIG(TAG, "SPIDevice started!");
}

void SPIDeviceComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SPIDevice");
  LOG_PIN("  CS pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Mode: %d", this->mode_);
  if (this->data_rate_ < 1000000) {
    ESP_LOGCONFIG(TAG, "  Data rate: %dkHz", this->data_rate_ / 1000);
  } else {
    ESP_LOGCONFIG(TAG, "  Data rate: %dMHz", this->data_rate_ / 1000000);
  }
}

float SPIDeviceComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace spi_device
}  // namespace esphome
