#include "airthings_wave_base.h"

#ifdef USE_ESP32

namespace esphome {
namespace airthings_wave_base {

static const char *const TAG = "airthings_wave_base";

void AirthingsWaveBase::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                            esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Connected successfully!");
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "Disconnected!");
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->handle_ = 0;
      auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->sensors_data_characteristic_uuid_);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", this->service_uuid_.to_string().c_str(),
                 this->sensors_data_characteristic_uuid_.to_string().c_str());
        break;
      }
      this->handle_ = chr->handle;
      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;

      this->request_read_values_();
      break;
    }

    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.conn_id != this->parent()->get_conn_id())
        break;
      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }
      if (param->read.handle == this->handle_) {
        this->read_sensors(param->read.value, param->read.value_len);
      }
      break;
    }

    default:
      break;
  }
}

bool AirthingsWaveBase::is_valid_voc_value_(uint16_t voc) { return 0 <= voc && voc <= 16383; }

void AirthingsWaveBase::update() {
  if (this->node_state != esp32_ble_tracker::ClientState::ESTABLISHED) {
    if (!this->parent()->enabled) {
      ESP_LOGW(TAG, "Reconnecting to device");
      this->parent()->set_enabled(true);
      this->parent()->connect();
    } else {
      ESP_LOGW(TAG, "Connection in progress");
    }
  }
}

void AirthingsWaveBase::request_read_values_() {
  auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->handle_,
                                        ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending read request for sensor, status=%d", status);
  }
}

}  // namespace airthings_wave_base
}  // namespace esphome

#endif  // USE_ESP32
