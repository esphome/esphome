#include "rojaflex_text_sensor.h"

#include <cstdio>
#include <string>

namespace esphome::rojaflex {

void RojaflexTextSensor::update() {
  if (this->parent_ == nullptr) {
    return;
  }
  switch (this->type_) {
    case RojaflexTextSensorType::STATUS:
      this->publish_state(this->parent_->get_device_status());
      break;
    case RojaflexTextSensorType::CONFIGURED_HOUSECODE:
      this->publish_state(this->parent_->get_housecode());
      break;
    case RojaflexTextSensorType::LAST_RX_RAW:
      this->publish_state(this->parent_->get_last_rx_raw());
      break;
    case RojaflexTextSensorType::LAST_RX_INFO:
      this->publish_state(this->parent_->get_last_rx_info());
      break;
    case RojaflexTextSensorType::LAST_TX_ERROR: {
      if (this->parent_->get_last_tx_ok()) {
        this->publish_state("NONE");
        break;
      }
      char error_buf[16];
      snprintf(error_buf, sizeof(error_buf), "%d", this->parent_->get_last_tx_error());
      this->publish_state(error_buf);
      break;
    }
    case RojaflexTextSensorType::CHANNEL_STATUS:
      this->publish_state(this->parent_->get_channel_status(this->channel_));
      break;
  }
}

}  // namespace esphome::rojaflex
