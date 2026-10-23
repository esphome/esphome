#include "ttp229_lsf.h"
#include "esphome/core/log.h"

namespace esphome::ttp229_lsf {

ESPHOME_LOG_TAG(TAG, "ttp229_lsf");

void TTP229LSFComponent::setup() {
  uint8_t data[2];
  if (this->read(data, 2) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
}
void TTP229LSFComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ttp229:");
  LOG_I2C_DEVICE(this);
}
void TTP229LSFComponent::loop() {
  uint16_t touched = 0;
  if (!this->status_comm_result(this->read(reinterpret_cast<uint8_t *>(&touched), 2) == i2c::ERROR_OK))
    return;
  touched = i2c::i2ctohs(touched);
  touched = reverse_bits(touched);
  for (auto *channel : this->channels_) {
    channel->process(touched);
  }
}

}  // namespace esphome::ttp229_lsf
