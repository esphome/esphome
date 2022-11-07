#include "ble_text_sensor.h"

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_client {

static const char *const TAG = "ble_text_sensor";

static const std::string EMPTY = "";

void BLETextSensor::loop() {}

void BLETextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "BLE Text Sensor", this);
  ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent()->address_str().c_str());
  ESP_LOGCONFIG(TAG, "  Service UUID       : %s", this->service_uuid_.to_string().c_str());
  ESP_LOGCONFIG(TAG, "  Characteristic UUID: %s", this->char_uuid_.to_string().c_str());
  ESP_LOGCONFIG(TAG, "  Descriptor UUID    : %s", this->descr_uuid_.to_string().c_str());
  ESP_LOGCONFIG(TAG, "  Notifications      : %s", YESNO(this->notify_));
  LOG_UPDATE_INTERVAL(this);
}

void BLETextSensor::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "[%s] Connected successfully!", this->get_name().c_str());
        break;
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "[%s] Disconnected!", this->get_name().c_str());
      this->status_set_warning();
      this->publish_state(EMPTY);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->handle = 0;
      auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
      if (chr == nullptr) {
        this->status_set_warning();
        this->publish_state(EMPTY);
        ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", this->service_uuid_.to_string().c_str(),
                 this->char_uuid_.to_string().c_str());
        break;
      }
      this->handle = chr->handle;
      if (this->descr_uuid_.get_uuid().len > 0) {
        auto *descr = chr->get_descriptor(this->descr_uuid_);
        if (descr == nullptr) {
          this->status_set_warning();
          this->publish_state(EMPTY);
          ESP_LOGW(TAG, "No sensor descriptor found at service %s char %s descr %s",
                   this->service_uuid_.to_string().c_str(), this->char_uuid_.to_string().c_str(),
                   this->descr_uuid_.to_string().c_str());
          break;
        }
        this->handle = descr->handle;
      }
      if (this->notify_) {
        auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                        this->parent()->get_remote_bda(), chr->handle);
        if (status) {
          ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
        }
      } else {
        this->node_state = espbt::ClientState::ESTABLISHED;
      }
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->parent()->get_conn_id())
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }
      if (param->read.handle == this->handle) {
        this->status_clear_warning();
        this->publish_state(this->parse_data(param->read.value, param->read.value_len));
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->parent()->get_conn_id() || param->notify.handle != this->handle)
        break;
      ESP_LOGV(TAG, "[%s] ESP_GATTC_NOTIFY_EVT: handle=0x%x, value=0x%x", this->get_name().c_str(),
               param->notify.handle, param->notify.value[0]);
      this->publish_state(this->parse_data(param->notify.value, param->notify.value_len));
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;
      break;
    }
    default:
      break;
  }
}

std::string BLETextSensor::parse_data(uint8_t *value, uint16_t value_len) {
  std::string text(value, value + value_len);
  return text;
}

void BLETextSensor::update() {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%s] Cannot poll, not connected", this->get_name().c_str());
    return;
  }
  if (this->handle == 0) {
    ESP_LOGW(TAG, "[%s] Cannot poll, no service or characteristic found", this->get_name().c_str());
    return;
  }

  auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->handle,
                                        ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    this->status_set_warning();
    this->publish_state(EMPTY);
    ESP_LOGW(TAG, "[%s] Error sending read request for sensor, status=%d", this->get_name().c_str(), status);
  }
}

}  // namespace ble_client
}  // namespace esphome
#endif
