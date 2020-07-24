#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/ble_client/sensor/ble_sensor.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ble_client {

class BLESensorNotifyTrigger : public Trigger<float>, public BLESensor {
 public:
  explicit BLESensorNotifyTrigger(BLESensor *sensor) { sensor_ = sensor; }
  void loop() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
    switch (event) {
      case ESP_GATTC_SEARCH_CMPL_EVT: {
        this->node_state_ = espbt::ClientState::Established;
        break;
      }
      case ESP_GATTC_NOTIFY_EVT: {
        if (param->notify.conn_id != this->parent_->conn_id_ || param->notify.handle != this->sensor_->sensor_handle_)
          break;
        this->trigger(this->parent_->parse_char_value(param->notify.value, param->notify.value_len));
      }
      default:
        break;
    }
  }

 protected:
  BLESensor *sensor_;
};

}  // namespace ble_client
}  // namespace esphome

#endif
