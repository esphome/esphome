#include "krida_i2c_dimmer.h"

#include "esphome/core/log.h"
#include <cmath>

const char *const TAG = "krida_i2c_dimmer_c";

namespace esphome {
namespace krida_i2c_dimmer {

void KridaI2CDimmer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up KridaDimmer (0x%02X)...", this->address_);
  auto err = this->write(nullptr, 0);
  if (err != i2c::ERROR_OK) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
}

void KridaI2CDimmer::dump_config() {
  LOG_I2C_DEVICE(this);

  if (this->error_code_ == COMMUNICATION_FAILED) {
    ESP_LOGE(TAG, "Communication with KridaDimmer failed!");
  }
}

void KridaI2CDimmer::write_state(float state) {
  const uint8_t value = std::trunc(100 - (state * 100));
  ESP_LOGI(TAG, "Updating dimmer value %i", value);
  this->write_register(channel_address_, &value, 1);
}

}  // namespace krida_i2c_dimmer
}  // namespace esphome
