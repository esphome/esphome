#include "spi_device.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome {
namespace spi_device {

static const char *const TAG = "spi_device";

void SPIDeviceComponent::setup() { this->spi_setup(); }

void SPIDeviceComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SPIDevice\n"
                "  Mode: %d",
                this->mode_);
  LOG_PIN("  CS pin: ", this->cs_);
  if (this->data_rate_ < 1000000) {
    ESP_LOGCONFIG(TAG, "  Data rate: %" PRId32 "kHz", this->data_rate_ / 1000);
  } else {
    ESP_LOGCONFIG(TAG, "  Data rate: %" PRId32 "MHz", this->data_rate_ / 1000000);
  }
}

float SPIDeviceComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace spi_device
}  // namespace esphome
