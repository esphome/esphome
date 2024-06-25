#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/ble_client/sensor/ble_sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_client {

class BLESensorNotifyTrigger : public Trigger<float>, public BLESensor {
 public:
  explicit BLESensorNotifyTrigger(BLESensor *sensor) { sensor_ = sensor; }
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    switch (event) {
      case ESP_GATTC_NOTIFY_EVT: {
        if (param->notify.handle == this->sensor_->handle)
          this->trigger(this->sensor_->parent()->parse_char_value(param->notify.value, param->notify.value_len));
        break;
      }
      case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        // confirms notifications are being listened for. While enabling of notifications may still be in
        // progress by the parent, we assume it will happen.
        if (param->reg_for_notify.status == ESP_GATT_OK && param->reg_for_notify.handle == this->sensor_->handle)
          this->node_state = espbt::ClientState::ESTABLISHED;
        break;
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
