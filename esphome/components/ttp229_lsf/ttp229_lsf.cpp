#include "ttp229_lsf.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ttp229_lsf {

static const char *const TAG = "ttp229_lsf";

void TTP229LSFComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ttp229...");
  uint8_t data[2];
  if (this->read(data, 2) != i2c::ERROR_OK) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
}
void TTP229LSFComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ttp229:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with TTP229 failed!");
      break;
    case NONE:
    default:
      break;
  }
}
void TTP229LSFComponent::loop() {
  uint16_t touched = 0;
  if (this->read(reinterpret_cast<uint8_t *>(&touched), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  touched = i2c::i2ctohs(touched);
  this->status_clear_warning();
  touched = reverse_bits_16(touched);
  for (auto *channel : this->channels_) {
    channel->process(touched);
  }
}

}  // namespace ttp229_lsf
}  // namespace esphome
