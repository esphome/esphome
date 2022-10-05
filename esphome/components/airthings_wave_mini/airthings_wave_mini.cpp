#include "airthings_wave_mini.h"

#ifdef USE_ESP32

namespace esphome {
namespace airthings_wave_mini {

static const char *const TAG = "airthings_wave_mini";

void AirthingsWaveMini::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
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
      auto *chr = this->parent()->get_characteristic(service_uuid_, sensors_data_characteristic_uuid_);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", service_uuid_.to_string().c_str(),
                 sensors_data_characteristic_uuid_.to_string().c_str());
        break;
      }
      this->handle_ = chr->handle;
      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;

      request_read_values_();
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
        read_sensors_(param->read.value, param->read.value_len);
      }
      break;
    }

    default:
      break;
  }
}

void AirthingsWaveMini::read_sensors_(uint8_t *raw_value, uint16_t value_len) {
  auto *value = (WaveMiniReadings *) raw_value;

  if (sizeof(WaveMiniReadings) <= value_len) {
    this->humidity_sensor_->publish_state(value->humidity / 100.0f);
    this->pressure_sensor_->publish_state(value->pressure / 50.0f);
    this->temperature_sensor_->publish_state(value->temperature / 100.0f - 273.15f);
    if (is_valid_voc_value_(value->voc)) {
      this->tvoc_sensor_->publish_state(value->voc);
    }

    // This instance must not stay connected
    // so other clients can connect to it (e.g. the
    // mobile app).
    parent()->set_enabled(false);
  }
}

bool AirthingsWaveMini::is_valid_voc_value_(uint16_t voc) { return 0 <= voc && voc <= 16383; }

void AirthingsWaveMini::update() {
  if (this->node_state != esp32_ble_tracker::ClientState::ESTABLISHED) {
    if (!parent()->enabled) {
      ESP_LOGW(TAG, "Reconnecting to device");
      parent()->set_enabled(true);
      parent()->connect();
    } else {
      ESP_LOGW(TAG, "Connection in progress");
    }
  }
}

void AirthingsWaveMini::request_read_values_() {
  auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->handle_,
                                        ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending read request for sensor, status=%d", status);
  }
}

void AirthingsWaveMini::dump_config() {
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
}

AirthingsWaveMini::AirthingsWaveMini()
    : PollingComponent(10000),
      service_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID)),
      sensors_data_characteristic_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(CHARACTERISTIC_UUID)) {}

}  // namespace airthings_wave_mini
}  // namespace esphome

#endif  // USE_ESP32
