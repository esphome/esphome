#include "husb238.h"
#include "esphome/core/log.h"

namespace esphome::husb238 {

ESPHOME_LOG_TAG(TAG, "husb238");

void Husb238Component::setup() {
  uint8_t status;
  if (!this->read_byte(REG_PD_STATUS1, &status)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  }
}

void Husb238Component::update() {
  uint8_t status;
  if (!this->status_comm_result(this->read_byte(REG_PD_STATUS1, &status)))
    return;

#ifdef USE_BINARY_SENSOR
  if (this->attached_binary_sensor_ != nullptr) {
    this->attached_binary_sensor_->publish_state((status & PD_STATUS1_ATTACHED) != 0);
  }
  if (this->cc_direction_binary_sensor_ != nullptr) {
    // The CC line only means something while a source is attached
    if (status & PD_STATUS1_ATTACHED) {
      this->cc_direction_binary_sensor_->publish_state((status & PD_STATUS1_CC_DIR) != 0);
    } else {
      this->cc_direction_binary_sensor_->invalidate_state();
    }
  }
#endif
}

void Husb238Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HUSB238:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "PD Attached", this->attached_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "CC Direction", this->cc_direction_binary_sensor_);
#endif
}

}  // namespace esphome::husb238
