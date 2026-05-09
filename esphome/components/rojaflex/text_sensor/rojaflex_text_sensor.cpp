#include "rojaflex_text_sensor.h"

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
    case RojaflexTextSensorType::LAST_TX_ERROR:
      this->publish_state(this->parent_->get_last_tx_ok() ? "NONE" : std::to_string(this->parent_->get_last_tx_error()));
      break;
    case RojaflexTextSensorType::CHANNEL_STATUS:
      this->publish_state(this->parent_->get_channel_status(this->channel_));
      break;
  }
}

}  // namespace esphome::rojaflex
