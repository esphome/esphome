#include "radon_eye_rd200.h"

#ifdef USE_ESP32

namespace esphome {
namespace radon_eye_rd200 {

static const char *const TAG = "radon_eye_rd200";

void RadonEyeRD200::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
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
      this->read_handle_ = 0;
      auto *chr = this->parent()->get_characteristic(service_uuid_, sensors_read_characteristic_uuid_);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "No sensor read characteristic found at service %s char %s", service_uuid_.to_string().c_str(),
                 sensors_read_characteristic_uuid_.to_string().c_str());
        break;
      }
      this->read_handle_ = chr->handle;

      // Write a 0x50 to the write characteristic.
      auto *write_chr = this->parent()->get_characteristic(service_uuid_, sensors_write_characteristic_uuid_);
      if (write_chr == nullptr) {
        ESP_LOGW(TAG, "No sensor write characteristic found at service %s char %s", service_uuid_.to_string().c_str(),
                 sensors_read_characteristic_uuid_.to_string().c_str());
        break;
      }
      this->write_handle_ = write_chr->handle;

      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;

      write_query_message_();

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
      if (param->read.handle == this->read_handle_) {
        read_sensors_(param->read.value, param->read.value_len);
      }
      break;
    }

    default:
      break;
  }
}

void RadonEyeRD200::read_sensors_(uint8_t *value, uint16_t value_len) {
  if (value_len < 20) {
    ESP_LOGD(TAG, "Invalid read");
    return;
  }

  // Example data
  // [13:08:47][D][radon_eye_rd200:107]: result bytes: 5010 85EBB940 00000000 00000000 2200 2500 0000
  ESP_LOGV(TAG, "result bytes: %02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X %02X%02X %02X%02X",
           value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7], value[8], value[9],
           value[10], value[11], value[12], value[13], value[14], value[15], value[16], value[17], value[18],
           value[19]);

  if (value[0] != 0x50) {
    // This isn't a sensor reading.
    return;
  }

  // Convert from pCi/L to Bq/m³
  constexpr float convert_to_bwpm3 = 37.0;

  RadonValue radon_value;
  radon_value.chars[0] = value[2];
  radon_value.chars[1] = value[3];
  radon_value.chars[2] = value[4];
  radon_value.chars[3] = value[5];
  float radon_now = radon_value.number * convert_to_bwpm3;
  if (is_valid_radon_value_(radon_now)) {
    radon_sensor_->publish_state(radon_now);
  }

  radon_value.chars[0] = value[6];
  radon_value.chars[1] = value[7];
  radon_value.chars[2] = value[8];
  radon_value.chars[3] = value[9];
  float radon_day = radon_value.number * convert_to_bwpm3;

  radon_value.chars[0] = value[10];
  radon_value.chars[1] = value[11];
  radon_value.chars[2] = value[12];
  radon_value.chars[3] = value[13];
  float radon_month = radon_value.number * convert_to_bwpm3;

  if (is_valid_radon_value_(radon_month)) {
    ESP_LOGV(TAG, "Radon Long Term based on month");
    radon_long_term_sensor_->publish_state(radon_month);
  } else if (is_valid_radon_value_(radon_day)) {
    ESP_LOGV(TAG, "Radon Long Term based on day");
    radon_long_term_sensor_->publish_state(radon_day);
  }

  ESP_LOGV(TAG, "  Measurements (Bq/m³) now: %0.03f, day: %0.03f, month: %0.03f", radon_now, radon_day, radon_month);

  ESP_LOGV(TAG, "  Measurements (pCi/L) now: %0.03f, day: %0.03f, month: %0.03f", radon_now / convert_to_bwpm3,
           radon_day / convert_to_bwpm3, radon_month / convert_to_bwpm3);

  // This instance must not stay connected
  // so other clients can connect to it (e.g. the
  // mobile app).
  parent()->set_enabled(false);
}

bool RadonEyeRD200::is_valid_radon_value_(float radon) { return radon > 0.0 and radon < 37000; }

void RadonEyeRD200::update() {
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

void RadonEyeRD200::write_query_message_() {
  ESP_LOGV(TAG, "writing 0x50 to write service");
  int request = 0x50;
  auto status = esp_ble_gattc_write_char_descr(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                               this->write_handle_, sizeof(request), (uint8_t *) &request,
                                               ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending write request for sensor, status=%d", status);
  }
}

void RadonEyeRD200::request_read_values_() {
  auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                        this->read_handle_, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending read request for sensor, status=%d", status);
  }
}

void RadonEyeRD200::dump_config() {
  LOG_SENSOR("  ", "Radon", this->radon_sensor_);
  LOG_SENSOR("  ", "Radon Long Term", this->radon_long_term_sensor_);
}

RadonEyeRD200::RadonEyeRD200()
    : PollingComponent(10000),
      service_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID)),
      sensors_write_characteristic_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(WRITE_CHARACTERISTIC_UUID)),
      sensors_read_characteristic_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(READ_CHARACTERISTIC_UUID)) {}

}  // namespace radon_eye_rd200
}  // namespace esphome

#endif  // USE_ESP32
