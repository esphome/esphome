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
    ESP_LOGCONFIG(TAG, "Channels currently open: %d",status);
}
void TCA9548AComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TCA9548A:");
  LOG_I2C_DEVICE(this);
  for (uint8_t i = 0; i < 8; i++) {
      ESP_LOGCONFIG(TAG, "Activating channel: %d",i);
      this->set_channel(i);
      this->parent_->dump_config();
  }

}

bool TCA9548AComponent::set_channel(uint8_t channelno) {
    uint8_t channelbyte = 1 << channelno;
    this->write_byte(0x70, channelbyte);

    return true;
}

}  // namespace tca9548a
}  // namespace esphome

