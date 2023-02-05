#include "fluval_ble_channel_number.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

static const char *const TAG = "fluval_ble_led_channel_number";

void FluvalBleChannelNumber::setup() { ESP_LOGD(TAG, "Setup called"); }

void FluvalBleChannelNumber::control(float value) {
  ESP_LOGD(TAG, "CHANNEL MODIFIED: %f", value);
  switch (this->channel_) {
    case 1:
      this->publish_state(this->parent_->get_status().channel1);
      break;
    case 2:
      this->publish_state(this->parent_->get_status().channel2);
      break;
    case 3:
      this->publish_state(this->parent_->get_status().channel3);
      break;
    case 4:
      this->publish_state(this->parent_->get_status().channel4);
      break;
    case 5:
      this->publish_state(this->parent_->get_status().channel5);
      break;
    default:
      break;
  }
  this->parent_->update_channel(this->channel_, value);
}

void FluvalBleChannelNumber::notify() {
  ESP_LOGD(TAG, "IN NUMBER NOTIFY: %d / zero_if_off: %d", this->channel_, this->zero_if_off_);

  if (this->zero_if_off_ && this->parent_->get_status().led_on_off == 0x00) {
    this->publish_state(0);
    return;
  }

  switch (this->channel_) {
    case 1:
      this->publish_state(this->parent_->get_status().channel1);
      break;
    case 2:
      this->publish_state(this->parent_->get_status().channel2);
      break;
    case 3:
      this->publish_state(this->parent_->get_status().channel3);
      break;
    case 4:
      this->publish_state(this->parent_->get_status().channel4);
      break;
    case 5:
      this->publish_state(this->parent_->get_status().channel5);
      break;
    default:
      break;
  }
}

void FluvalBleChannelNumber::dump_config() { LOG_NUMBER("", "Fluval BLE Channel Number", this); }

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
