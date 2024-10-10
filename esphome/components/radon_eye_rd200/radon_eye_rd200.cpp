#include "radon_eye_rd200.h"

#ifdef USE_ESP32

namespace esphome {
namespace radon_eye_rd200 {

static const char *const TAG = "radon_eye_rd200";

void RadonEyeRD200::handle_status_response_(const uint8_t *response, uint16_t length) {
  if (response[0] != 0x40) {
    // This isn't a sensor reading.
    return;
  }

  radoneye_value_response_t *data = (radoneye_value_response_t *) response;

  ESP_LOGD(TAG, "  Model: %.6s", data->model);
  ESP_LOGD(TAG, "  Version: %.6s", data->version);
  if (data->unit == 0) {
    ESP_LOGD(TAG, "  Unit: pCi/ℓ");
    ESP_LOGD(TAG, "  Measurements (pCi/ℓ) now: %u", data->latest_bq_m3);
    ESP_LOGD(TAG, "  Measurements Day (pCi/ℓ) avg: %u", data->day_avg_bq_m3);
    ESP_LOGD(TAG, "  Measurements Month (pCi/ℓ) avg: %u", data->month_avg_bq_m3);
    ESP_LOGD(TAG, "  Measurements Peak (pCi/ℓ): %u", data->peak_bq_m3);
  } else {
    ESP_LOGD(TAG, "  Unit: Bq/m³");
    ESP_LOGD(TAG, "  Measurements (Bq/m³) now: %u", data->latest_bq_m3);
    ESP_LOGD(TAG, "  Measurements Day (Bq/m³) avg: %u", data->day_avg_bq_m3);
    ESP_LOGD(TAG, "  Measurements Month (Bq/m³) avg: %u", data->month_avg_bq_m3);
    ESP_LOGD(TAG, "  Measurements Peak (Bq/m³): %u", data->peak_bq_m3);
  }
  if (data->alarm != 0) {
    ESP_LOGD(TAG, "  AlarmStatus on!");
  } else {
    ESP_LOGD(TAG, "  AlarmStatus off");
  }
  ESP_LOGD(TAG, "  Alarm Value: %u", data->alarm_value);

  if (data->alarm_interval == 1) {
    ESP_LOGD(TAG, "  Alarm Interval: 10min");
  } else if (data->alarm_interval == 6) {
    ESP_LOGD(TAG, "  Alarm Interval: 6H");
  } else if (data->alarm_interval == 24) {
    ESP_LOGD(TAG, "  Alarm Interval: 24H");
  }

  if (this->radon_version_ == 3) {
    ESP_LOGD(TAG, "  Barcode %.6s%.3s%.4s", (char *) data->serial_part2, (char *) data->serial_part1,
             (char *) data->serial_part3);
  } else {
    ESP_LOGD(TAG, "  Barcode %.3s%.6s%.4s", (char *) data->serial_part1, (char *) data->serial_part2,
             (char *) data->serial_part3);
  }

  ESP_LOGV(TAG, "  HexData: %s", format_hex_pretty(response, length).c_str());

  float flatest_bq_m3 = (float) data->latest_bq_m3;
  float fday_avg_bq_m3 = (float) data->day_avg_bq_m3;
  float fmonth_avg_bq_m3 = (float) data->month_avg_bq_m3;
  float fpeak_bq_m3 = (float) data->peak_bq_m3;

  if (radon_sensor_ != nullptr) {
    ESP_LOGD(TAG, " Sensor radon send!");
    radon_sensor_->publish_state(flatest_bq_m3);
    delay(25);
  } else {
    ESP_LOGI(TAG, " Sensor radon not exists!");
  }
  if (radon_day_avg_ != nullptr) {
    ESP_LOGD(TAG, " Sensor radon_day_avg send!");
    radon_day_avg_->publish_state(fday_avg_bq_m3);
    delay(25);
  } else {
    ESP_LOGI(TAG, " Sensor radon_day_avg not exists!");
  }
  if (radon_long_term_sensor_ != nullptr) {
    ESP_LOGD(TAG, " Sensor radon_long_term send!");
    radon_long_term_sensor_->publish_state(fmonth_avg_bq_m3);
    delay(25);
  } else {
    ESP_LOGI(TAG, " Sensor radon_long_term not exists!");
  }
  if (radon_peak_ != nullptr) {
    ESP_LOGD(TAG, " Sensor radon_peak send!");
    radon_peak_->publish_state(fpeak_bq_m3);
    delay(25);
  } else {
    ESP_LOGI(TAG, " Sensor radon_peak not exists!");
  }
  if (alarm_sensor_ != nullptr) {
    ESP_LOGD(TAG, " Sensor alarm send!");
    if (data->alarm != 0) {
      alarm_sensor_->publish_state(true);
    } else {
      alarm_sensor_->publish_state(false);
    }

    delay(25);
  } else {
    ESP_LOGI(TAG, " Sensor alarm not exists!");
  }

  return;
}

void RadonEyeRD200::handle_history_response_(const uint8_t *response, uint16_t length) { return; }

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
      if (this->radon_version_ == 2 || this->radon_version_ == 3) {
        ESP_LOGW(TAG, "Use Version 2 or 3");
        this->read_handle_ = 0;
        auto *command_chr =
            this->parent()->get_characteristic(service_uuid_v2_, sensors_command_characteristic_uuid_v2_);
        if (command_chr == nullptr) {
          ESP_LOGW(TAG, "No sensor command characteristic found at service %s char %s",
                   service_uuid_v2_.to_string().c_str(), sensors_command_characteristic_uuid_v2_.to_string().c_str());
          break;
        } else {
          ESP_LOGD(TAG, "Connect Command_Handler");
        }
        this->command_handle_ = command_chr->handle;

        auto *status_chr = this->parent()->get_characteristic(service_uuid_v2_, sensors_status_characteristic_uuid_v2_);
        if (status_chr == nullptr) {
          ESP_LOGW(TAG, "No sensor status characteristic found at service %s char %s",
                   service_uuid_v2_.to_string().c_str(), sensors_status_characteristic_uuid_v2_.to_string().c_str());
          break;
        } else {
          ESP_LOGD(TAG, "Connect Status_Handler");
        }
        this->status_handle_ = status_chr->handle;

        auto *history_chr =
            this->parent()->get_characteristic(service_uuid_v2_, sensors_history_characteristic_uuid_v2_);
        if (history_chr == nullptr) {
          ESP_LOGW(TAG, "No sensor history characteristic found at service %s char %s",
                   service_uuid_v2_.to_string().c_str(), sensors_history_characteristic_uuid_v2_.to_string().c_str());
          break;
        } else {
          ESP_LOGD(TAG, "Connect History_Handler");
        }
        this->history_handle_ = status_chr->handle;

        auto sensor_status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                                               this->parent()->get_remote_bda(), this->status_handle_);
        if (sensor_status) {
          ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify sensor status failed, status=%d", sensor_status);
        } else {
          ESP_LOGD(TAG, "Register for Notify Status_Handler");
        }

        auto sensor_history = esp_ble_gattc_register_for_notify(
            this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), this->history_handle_);
        if (sensor_history) {
          ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify sensor history failed, status=%d", sensor_history);
        } else {
          ESP_LOGD(TAG, "Register for Notify History_Handler");
        }

        this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;

        write_status_query_message_();

        break;

      } else {
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
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->notify.handle == this->status_handle_) {
        this->status_handle_state_ = esp32_ble_tracker::ClientState::ESTABLISHED;
      }
      if (param->notify.handle == this->history_handle_) {
        this->history_handle_state_ = esp32_ble_tracker::ClientState::ESTABLISHED;
      }

      if (this->status_handle_state_ == esp32_ble_tracker::ClientState::ESTABLISHED &&
          this->history_handle_state_ == esp32_ble_tracker::ClientState::ESTABLISHED) {
        this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->status_handle_) {
        ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT: handle=0x%x, value=0x%x", param->notify.handle, param->notify.value[0]);

        this->handle_status_response_(param->notify.value, param->notify.value_len);
      }
      if (param->notify.handle == this->history_handle_) {
        ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT: handle=0x%x, value=0x%x", param->notify.handle, param->notify.value[0]);

        this->handle_history_response_(param->notify.value, param->notify.value_len);
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
  } else {
    if (this->radon_version_ == 2 || this->radon_version_ == 3) {
      ESP_LOGV(TAG, "Update Version 2 or 3");
      write_status_query_message_();
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

void RadonEyeRD200::write_status_query_message_() {
  ESP_LOGD(TAG, "writing 0x40 to command service");
  int request = 0x40;
  auto status = esp_ble_gattc_write_char_descr(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                               this->command_handle_, sizeof(request), (uint8_t *) &request,
                                               ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending command request for sensor, status=%d", status);
  }
}

void RadonEyeRD200::write_history_query_message_() {
  ESP_LOGD(TAG, "writing 0x41 to command service");
  int request = 0x41;
  auto status = esp_ble_gattc_write_char_descr(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                               this->command_handle_, sizeof(request), (uint8_t *) &request,
                                               ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending command request for sensor, status=%d", status);
  }
}

void RadonEyeRD200::write_beep_query_message_() {
  ESP_LOGD(TAG, "writing 0xA1 to command service");
  int request = 0xA1;
  auto status = esp_ble_gattc_write_char_descr(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                               this->command_handle_, sizeof(request), (uint8_t *) &request,
                                               ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending command request for sensor, status=%d", status);
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
  ESP_LOGCONFIG(TAG, "Radon Eye RD200:");
  LOG_SENSOR("  ", "Radon", this->radon_sensor_);
  LOG_SENSOR("  ", "Radon Long Term", this->radon_long_term_sensor_);
  LOG_SENSOR("  ", "Radon Day Avg", this->radon_day_avg_);
  LOG_SENSOR("  ", "Radon Peak", this->radon_peak_);
}

RadonEyeRD200::RadonEyeRD200()
    : PollingComponent(10000),
      service_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID)),
      service_uuid_v2_(esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID_V2)),
      sensors_write_characteristic_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(WRITE_CHARACTERISTIC_UUID)),
      sensors_read_characteristic_uuid_(esp32_ble_tracker::ESPBTUUID::from_raw(READ_CHARACTERISTIC_UUID)),
      sensors_command_characteristic_uuid_v2_(esp32_ble_tracker::ESPBTUUID::from_raw(COMMAND_CHARACTERISTIC_UUID_V2)),
      sensors_status_characteristic_uuid_v2_(esp32_ble_tracker::ESPBTUUID::from_raw(STATUS_CHARACTERISTIC_UUID_V2)),
      sensors_history_characteristic_uuid_v2_(esp32_ble_tracker::ESPBTUUID::from_raw(HISTROY_CHARACTERISTIC_UUID_V2)) {}

}  // namespace radon_eye_rd200
}  // namespace esphome

#endif  // USE_ESP32
