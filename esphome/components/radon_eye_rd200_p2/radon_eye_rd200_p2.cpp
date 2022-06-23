#include "radon_eye_rd200_p2.h"

#ifdef USE_ESP32

namespace esphome {
namespace radon_eye_rd200_p2 {

static const char *const TAG = "radon_eye_rd200_p2";

void RadonEyeRD200P2::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_CONNECT_EVT: {
      ESP_LOGD(TAG, "Connected successfully.");
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "Disconnected.");
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGD(TAG, "GATTC Search completed.");

      bool has_required_chars = this->has_required_chars_();
      if (has_required_chars) {
        ESP_LOGD(TAG, "Device has the required services.");
        // We first ask by the radon measurement
        this->write_query_radon_message_();
      } else {
        ESP_LOGD(TAG, "Device doesn't have the required services. Disconnect.");
        parent()->set_enabled(false);
      }

      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
      ESP_LOGD(TAG, "Write char event received.");

      //  Write operation completed, now we read the result.
      request_read_values_();
      break;
    }

    case ESP_GATTC_READ_CHAR_EVT: {
      ESP_LOGD(TAG, "Read char event received.");

      if (param->read.conn_id != this->parent()->conn_id) {
        ESP_LOGD(TAG, "param->read.conn_id (%d) != this->parent()->conn_id (%d)", param->read.conn_id,
                 this->parent()->conn_id);
        break;
      }

      if (param->read.status != ESP_GATT_OK) {
        ESP_LOGD(TAG, "Error reading char at handle %d, status=%d", param->read.handle, param->read.status);
        break;
      }
      if (param->read.handle == this->read_handle_) {
        if (this->current_query_ == QUERY_RADON) {
          ESP_LOGD(TAG, "Previous query was query_radon");
          this->read_sensors_radon_(param->read.value, param->read.value_len);
          this->write_query_temp_hum_message_();
        } else if (this->current_query_ == QUERY_TEMP_HUM) {
          ESP_LOGD(TAG, "Previous query was query_temp_hum");
          this->read_sensors_temp_hum_(param->read.value, param->read.value_len);
        }
      }
      break;
    }

    default:
      ESP_LOGD(TAG, "gattc_event_handler new event: %d", (int) event);
      break;
  }
}

bool RadonEyeRD200P2::has_required_chars_() {
  this->read_handle_ = 0;
  auto *chr = this->parent()->get_characteristic(service_uuid_, sensors_read_characteristic_uuid_);
  if (chr == nullptr) {
    ESP_LOGD(TAG, "No sensor read characteristic found at service %s char %s", service_uuid_.to_string().c_str(),
             sensors_read_characteristic_uuid_.to_string().c_str());
    return false;
  }
  this->read_handle_ = chr->handle;

  auto *write_chr = this->parent()->get_characteristic(service_uuid_, sensors_write_characteristic_uuid_);
  if (write_chr == nullptr) {
    ESP_LOGD(TAG, "No sensor write characteristic found at service %s char %s", service_uuid_.to_string().c_str(),
             sensors_read_characteristic_uuid_.to_string().c_str());
    return false;
  }
  this->write_handle_ = write_chr->handle;
  this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
  return true;
}

void RadonEyeRD200P2::read_sensors_radon_(uint8_t *value, uint16_t value_len) {
  if (value_len >= 20) {
    ESP_LOGD(
        TAG, "Result bytes: [%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X %02X%02X %02X%02X].",
        value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7], value[8], value[9], value[10],
        value[11], value[12], value[13], value[14], value[15], value[16], value[17], value[18], value[19]);
  }

  if (value_len < 20 || (value[0] != 0x50)) {
    ESP_LOGD(TAG, "Parsing data: It is not a valid radon measurement.");
  } else {
    int radon_value = value[2] | value[3] << 8;
    radon_sensor_->publish_state(radon_value);
    ESP_LOGI(TAG, "  Radon (Bq/m³): %d", radon_value);
  }
}

void RadonEyeRD200P2::read_sensors_temp_hum_(uint8_t *value, uint16_t value_len) {
  if (value_len >= 20) {
    ESP_LOGD(
        TAG, "Result bytes: [%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X %02X%02X %02X%02X].",
        value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7], value[8], value[9], value[10],
        value[11], value[12], value[13], value[14], value[15], value[16], value[17], value[18], value[19]);
  }

  if (value_len < 20 || (value[0] != 0x51)) {
    ESP_LOGD(TAG, "Parsing data: It is not a valid temperature-humidity measurement.");
  } else {
    int temp_hum_value = value[12] | value[13] << 8;
    float temperature = temphum_u16_to_temperature_(temp_hum_value);
    int humidity = temphum_u16_to_humidity_(temp_hum_value);

    temperature_sensor_->publish_state(temperature);
    humidity_sensor_->publish_state(humidity);

    ESP_LOGI(TAG, "  Temperature (C): %0.01f Humidity: %d", temperature, humidity);
  }

  // All data retrieved, disconnect.
  parent()->set_enabled(false);
}

float RadonEyeRD200P2::temphum_u16_to_temperature_(uint16_t value) {
  uint16_t i2 = value >> 8;
  if (((value >> 7) & 1) == 1) {
    return i2 + 0.5;
  };
  return i2;
}

uint16_t RadonEyeRD200P2::temphum_u16_to_humidity_(uint16_t value) {
  uint16_t i2 = (value % 256);
  if (i2 <= 0) {
    i2 += 256;
  }
  return (i2 % 128);
}

void RadonEyeRD200P2::update() {
  if (this->node_state != esp32_ble_tracker::ClientState::ESTABLISHED) {
    if (!parent()->enabled) {
      ESP_LOGD(TAG, "Reconnecting to device...");
      parent()->set_enabled(true);
      parent()->connect();
    } else {
      ESP_LOGD(TAG, "Already connected while updating.");
    }
  }
}

void RadonEyeRD200P2::write_query_radon_message_() {
  this->current_query_ = QUERY_RADON;

  uint8_t request = 0x50;
  ESP_LOGD(TAG, "Writing %d to write service", request);
  auto status =
      esp_ble_gattc_write_char(this->parent()->gattc_if, this->parent()->conn_id, this->write_handle_, sizeof(request),
                               (uint8_t *) &request, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGD(TAG, "Error sending write request for sensor, status=%d.", status);
  }
}

void RadonEyeRD200P2::write_query_temp_hum_message_() {
  this->current_query_ = QUERY_TEMP_HUM;
  uint8_t request = 0x51;
  ESP_LOGD(TAG, "Writing %d to write service", request);
  auto status =
      esp_ble_gattc_write_char(this->parent()->gattc_if, this->parent()->conn_id, this->write_handle_, sizeof(request),
                               (uint8_t *) &request, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGD(TAG, "Error sending write request for sensor, status=%d.", status);
  }
}

void RadonEyeRD200P2::request_read_values_() {
  auto status = esp_ble_gattc_read_char(this->parent()->gattc_if, this->parent()->conn_id, this->read_handle_,
                                        ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGD(TAG, "Error sending read request for sensor, status=%d.", status);
  }
}

void RadonEyeRD200P2::dump_config() {
  LOG_SENSOR("  ", "Radon", this->radon_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

RadonEyeRD200P2::RadonEyeRD200P2()
    : PollingComponent(10000),
      service_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID)),
      sensors_write_characteristic_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(WRITE_CHARACTERISTIC_UUID)),
      sensors_read_characteristic_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(READ_CHARACTERISTIC_UUID)) {}

}  // namespace radon_eye_rd200_p2
}  // namespace esphome

#endif  // USE_ESP32
