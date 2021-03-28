#include "tca9548a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tca9548a {

static const char *TAG = "tca9548a";

void TCA9548AComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TCA9548A...");
  uint8_t status = 0;
  if (!this->read_byte(0x00, &status)) {
    ESP_LOGI(TAG, "TCA9548A failed");
    return;
  }
  // out of range to make sure on first set_channel a new one will be set
  this->current_channelno_ = 8;
  ESP_LOGCONFIG(TAG, "Channels currently open: %d", status);
}
void TCA9548AComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TCA9548A:");
  LOG_I2C_DEVICE(this);
  if (this->scan_) {
    for (uint8_t i = 0; i < 8; i++) {
      ESP_LOGCONFIG(TAG, "Activating channel: %d", i);
      this->set_channel(i);
      this->parent_->dump_config();
    }
  }
}

void TCA9548AComponent::set_channel(uint8_t channelno) {
  if (this->current_channelno_ != channelno) {
    this->current_channelno_ = channelno;
    uint8_t channelbyte = 1 << channelno;
    this->write_byte(0x70, channelbyte);
  }
}

}  // namespace tca9548a
}  // namespace esphome
