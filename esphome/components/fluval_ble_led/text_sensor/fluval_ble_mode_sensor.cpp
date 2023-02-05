#include "fluval_ble_mode_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

static const char *const TAG = "fluval_ble_led_mode_sensor";

void FluvalBleModeSensor::setup() { ESP_LOGD(TAG, "Setup called"); }

void FluvalBleModeSensor::notify() {
  uint8_t state = this->parent_->get_status().mode;
  ESP_LOGV(TAG, "In notify. State: %d", state);

  switch (state) {
    case fluval_ble_led::MANUAL_MODE:
      this->publish_state(this->manual_mapping_);
      break;
    case fluval_ble_led::AUTO_MODE:
      this->publish_state(this->auto_mapping_);
      break;
    case fluval_ble_led::PRO_MODE:
      this->publish_state(this->pro_mapping_);
      break;
    default:
      this->publish_state("unknown");
      break;
  }
}

void FluvalBleModeSensor::dump_config() { LOG_TEXT_SENSOR("", "Fluval BLE Mode Sensor", this); }

}  // namespace fluval_ble_led
}  // namespace esphome
#endif
