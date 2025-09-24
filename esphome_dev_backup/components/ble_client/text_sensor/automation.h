#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/ble_client/text_sensor/ble_text_sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_client {

class BLETextSensorNotifyTrigger : public Trigger<std::string>, public BLETextSensor {
 public:
  explicit BLETextSensorNotifyTrigger(BLETextSensor *sensor) { sensor_ = sensor; }
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    switch (event) {
      case ESP_GATTC_SEARCH_CMPL_EVT: {
        this->sensor_->node_state = espbt::ClientState::ESTABLISHED;
        break;
      }
      case ESP_GATTC_NOTIFY_EVT: {
        if (param->notify.conn_id != this->sensor_->parent()->get_conn_id() ||
            param->notify.handle != this->sensor_->handle)
          break;
        this->trigger(this->sensor_->parse_data(param->notify.value, param->notify.value_len));
      }
      default:
        break;
    }
  }

 protected:
  BLETextSensor *sensor_;
};

}  // namespace ble_client
}  // namespace esphome

#endif
