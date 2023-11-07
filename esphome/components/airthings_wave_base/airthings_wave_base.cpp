#include "airthings_wave_base.h"

// All information related to reading battery information came from the sensors.airthings_wave
// project by Sverre Hamre (https://github.com/sverrham/sensor.airthings_wave)

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
      this->handle_ = 0;
      this->acp_handle_ = 0;
      this->cccd_handle_ = 0;
      ESP_LOGW(TAG, "Disconnected!");
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (this->request_read_values_()) {
        if (!this->read_battery_next_update_) {
          this->node_state = espbt::ClientState::ESTABLISHED;
        } else {
          // delay setting node_state to ESTABLISHED until confirmation of the notify registration
          this->request_battery_();
        }
      }

      // ensure that the client will be disconnected even if no responses arrive
      this->set_response_timeout_();

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

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.conn_id != this->parent()->get_conn_id())
        break;
      if (param->notify.handle == this->acp_handle_) {
        this->read_battery_(param->notify.value, param->notify.value_len);
      }
      break;
    }

    default:
      break;
  }
}

bool AirthingsWaveBase::is_valid_voc_value_(uint16_t voc) { return voc <= 16383; }

void AirthingsWaveBase::update() {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    if (!this->parent()->enabled) {
      ESP_LOGW(TAG, "Reconnecting to device");
      this->parent()->set_enabled(true);
      this->parent()->connect();
    } else {
      ESP_LOGW(TAG, "Connection in progress");
    }
  }
}

bool AirthingsWaveBase::request_read_values_() {
  auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->sensors_data_characteristic_uuid_);
  if (chr == nullptr) {
    ESP_LOGW(TAG, "No sensor characteristic found at service %s char %s", this->service_uuid_.to_string().c_str(),
             this->sensors_data_characteristic_uuid_.to_string().c_str());
    return false;
  }

  this->handle_ = chr->handle;

  auto status = esp_ble_gattc_read_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->handle_,
                                        ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending read request for sensor, status=%d", status);
    return false;
  }

  this->response_pending_();
  return true;
}

bool AirthingsWaveBase::request_battery_() {
  uint8_t battery_command = ACCESS_CONTROL_POINT_COMMAND;
  uint8_t cccd_value[2] = {1, 0};

  auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->access_control_point_characteristic_uuid_);
  if (chr == nullptr) {
    ESP_LOGW(TAG, "No access control point characteristic found at service %s char %s",
             this->service_uuid_.to_string().c_str(),
             this->access_control_point_characteristic_uuid_.to_string().c_str());
    return false;
  }

  auto *descr = this->parent()->get_descriptor(this->service_uuid_, this->access_control_point_characteristic_uuid_,
                                               CLIENT_CHARACTERISTIC_CONFIGURATION_DESCRIPTOR_UUID);
  if (descr == nullptr) {
    ESP_LOGW(TAG, "No CCC descriptor found at service %s char %s", this->service_uuid_.to_string().c_str(),
             this->access_control_point_characteristic_uuid_.to_string().c_str());
    return false;
  }

  auto reg_status =
      esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), chr->handle);
  if (reg_status) {
    ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", reg_status);
    return false;
  }

  this->acp_handle_ = chr->handle;
  this->cccd_handle_ = descr->handle;

  auto descr_status =
      esp_ble_gattc_write_char_descr(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->cccd_handle_,
                                     2, cccd_value, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (descr_status) {
    ESP_LOGW(TAG, "Error sending CCC descriptor write request, status=%d", descr_status);
    return false;
  }

  auto chr_status =
      esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->acp_handle_, 1,
                               &battery_command, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (chr_status) {
    ESP_LOGW(TAG, "Error sending read request for battery, status=%d", chr_status);
    return false;
  }

  this->response_pending_();
  return true;
}

void AirthingsWaveBase::read_battery_(uint8_t *raw_value, uint16_t value_len) {
  auto *value = (AccessControlPointResponse *) (&raw_value[2]);

  if ((value_len >= (sizeof(AccessControlPointResponse) + 2)) && (raw_value[0] == ACCESS_CONTROL_POINT_COMMAND)) {
    ESP_LOGD(TAG, "Battery received: %u mV", (unsigned int) value->battery);

    if (this->battery_voltage_ != nullptr) {
      float voltage = value->battery / 1000.0f;

      this->battery_voltage_->publish_state(voltage);
    }

    // read the battery again at the configured update interval
    if (this->battery_update_interval_ != this->update_interval_) {
      this->read_battery_next_update_ = false;
      this->set_timeout("battery", this->battery_update_interval_,
                        [this]() { this->read_battery_next_update_ = true; });
    }
  }

  this->response_received_();
}

void AirthingsWaveBase::response_pending_() {
  this->responses_pending_++;
  this->set_response_timeout_();
}

void AirthingsWaveBase::response_received_() {
  if (--this->responses_pending_ == 0) {
    // This instance must not stay connected
    // so other clients can connect to it (e.g. the
    // mobile app).
    this->parent()->set_enabled(false);
  }
}

void AirthingsWaveBase::set_response_timeout_() {
  this->set_timeout("response_timeout", 30 * 1000, [this]() {
    this->responses_pending_ = 1;
    this->response_received_();
  });
}

}  // namespace airthings_wave_base
}  // namespace esphome

#endif  // USE_ESP32
