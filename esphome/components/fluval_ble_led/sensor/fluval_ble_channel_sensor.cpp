#include "fluval_ble_channel_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

static const char *const TAG = "fluval_ble_led_channel_sensor";

void FluvalBleChannelSensor::setup() { ESP_LOGD(TAG, "Setup called"); }

void FluvalBleChannelSensor::notify() {
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

void FluvalBleChannelSensor::dump_config() { LOG_SENSOR("", "Fluval BLE Channel Sensor", this); }

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
