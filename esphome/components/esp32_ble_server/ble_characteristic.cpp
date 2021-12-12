#include "ble_characteristic.h"
#include "ble_server.h"
#include "ble_service.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_server {

static const char *const TAG = "esp32_ble_server.characteristic";

BLECharacteristic::BLECharacteristic(const ESPBTUUID uuid, uint32_t properties) : uuid_(uuid) {
  this->set_value_lock_ = xSemaphoreCreateBinary();
  xSemaphoreGive(this->set_value_lock_);

  this->properties_ = (esp_gatt_char_prop_t) 0;

  this->set_broadcast_property((properties & PROPERTY_BROADCAST) != 0);
  this->set_indicate_property((properties & PROPERTY_INDICATE) != 0);
  this->set_notify_property((properties & PROPERTY_NOTIFY) != 0);
  this->set_read_property((properties & PROPERTY_READ) != 0);
  this->set_write_property((properties & PROPERTY_WRITE) != 0);
  this->set_write_no_response_property((properties & PROPERTY_WRITE_NR) != 0);
}

void BLECharacteristic::set_value(std::vector<uint8_t> value) {
  xSemaphoreTake(this->set_value_lock_, 0L);
  this->value_ = std::move(value);
  xSemaphoreGive(this->set_value_lock_);
}
void BLECharacteristic::set_value(const std::string &value) {
  this->set_value(std::vector<uint8_t>(value.begin(), value.end()));
}
void BLECharacteristic::set_value(const uint8_t *data, size_t length) {
  this->set_value(std::vector<uint8_t>(data, data + length));
}
void BLECharacteristic::set_value(uint8_t &data) {
  uint8_t temp[1];
  temp[0] = data;
  this->set_value(temp, 1);
}
void BLECharacteristic::set_value(uint16_t &data) {
  uint8_t temp[2];
  temp[0] = data;
  temp[1] = data >> 8;
  this->set_value(temp, 2);
}
void BLECharacteristic::set_value(uint32_t &data) {
  uint8_t temp[4];
  temp[0] = data;
  temp[1] = data >> 8;
  temp[2] = data >> 16;
  temp[3] = data >> 24;
  this->set_value(temp, 4);
}
void BLECharacteristic::set_value(int &data) {
  uint8_t temp[4];
  temp[0] = data;
  temp[1] = data >> 8;
  temp[2] = data >> 16;
  temp[3] = data >> 24;
  this->set_value(temp, 4);
}
void BLECharacteristic::set_value(float &data) {
  float temp = data;
  this->set_value((uint8_t *) &temp, 4);
}
void BLECharacteristic::set_value(double &data) {
  double temp = data;
  this->set_value((uint8_t *) &temp, 8);
}
void BLECharacteristic::set_value(bool &data) {
  uint8_t temp[1];
  temp[0] = data;
  this->set_value(temp, 1);
}

void BLECharacteristic::notify(bool notification) {
  if (!notification) {
    ESP_LOGW(TAG, "notification=false is not yet supported");
    // TODO: Handle when notification=false
  }
  if (this->service_->get_server()->get_connected_client_count() == 0)
    return;

  for (auto &client : this->service_->get_server()->get_clients()) {
    size_t length = this->value_.size();
    esp_err_t err = esp_ble_gatts_send_indicate(this->service_->get_server()->get_gatts_if(), client.first,
                                                this->handle_, length, this->value_.data(), false);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gatts_send_indicate failed %d", err);
      return;
    }
  }
}

void BLECharacteristic::add_descriptor(BLEDescriptor *descriptor) { this->descriptors_.push_back(descriptor); }

void BLECharacteristic::do_create(BLEService *service) {
  this->service_ = service;
  esp_attr_control_t control;
  control.auto_rsp = ESP_GATT_RSP_BY_APP;

  ESP_LOGV(TAG, "Creating characteristic - %s", this->uuid_.to_string().c_str());

  esp_bt_uuid_t uuid = this->uuid_.get_uuid();
  esp_err_t err = esp_ble_gatts_add_char(service->get_handle(), &uuid, static_cast<esp_gatt_perm_t>(this->permissions_),
                                         this->properties_, nullptr, &control);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gatts_add_char failed: %d", err);
    return;
  }

  this->state_ = CREATING;
}

bool BLECharacteristic::is_created() {
  if (this->state_ == CREATED)
    return true;

  if (this->state_ != CREATING_DEPENDENTS)
    return false;

  bool created = true;
  for (auto *descriptor : this->descriptors_) {
    created &= descriptor->is_created();
  }
  if (created)
    this->state_ = CREATED;
  return this->state_ == CREATED;
}

bool BLECharacteristic::is_failed() {
  if (this->state_ == FAILED)
    return true;

  bool failed = false;
  for (auto *descriptor : this->descriptors_) {
    failed |= descriptor->is_failed();
  }
  if (failed)
    this->state_ = FAILED;
  return this->state_ == FAILED;
}

void BLECharacteristic::set_broadcast_property(bool value) {
  if (value)
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ | ESP_GATT_CHAR_PROP_BIT_BROADCAST);
  else
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ & ~ESP_GATT_CHAR_PROP_BIT_BROADCAST);
}
void BLECharacteristic::set_indicate_property(bool value) {
  if (value)
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ | ESP_GATT_CHAR_PROP_BIT_INDICATE);
  else
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ & ~ESP_GATT_CHAR_PROP_BIT_INDICATE);
}
void BLECharacteristic::set_notify_property(bool value) {
  if (value)
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
  else
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ & ~ESP_GATT_CHAR_PROP_BIT_NOTIFY);
}
void BLECharacteristic::set_read_property(bool value) {
  if (value)
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ | ESP_GATT_CHAR_PROP_BIT_READ);
  else
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ & ~ESP_GATT_CHAR_PROP_BIT_READ);
}
void BLECharacteristic::set_write_property(bool value) {
  if (value)
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ | ESP_GATT_CHAR_PROP_BIT_WRITE);
  else
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ & ~ESP_GATT_CHAR_PROP_BIT_WRITE);
}
void BLECharacteristic::set_write_no_response_property(bool value) {
  if (value)
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ | ESP_GATT_CHAR_PROP_BIT_WRITE_NR);
  else
    this->properties_ = (esp_gatt_char_prop_t)(this->properties_ & ~ESP_GATT_CHAR_PROP_BIT_WRITE_NR);
}

void BLECharacteristic::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                            esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_ADD_CHAR_EVT: {
      if (this->uuid_ == ESPBTUUID::from_uuid(param->add_char.char_uuid)) {
        this->handle_ = param->add_char.attr_handle;

        for (auto *descriptor : this->descriptors_) {
          descriptor->do_create(this);
        }

        this->state_ = CREATING_DEPENDENTS;
      }
      break;
    }
    case ESP_GATTS_READ_EVT: {
      if (param->read.handle != this->handle_)
        break;  // Not this characteristic

      if (!param->read.need_rsp)
        break;  // For some reason you can request a read but not want a response

      uint16_t max_offset = 22;

      esp_gatt_rsp_t response;
      if (param->read.is_long) {
        if (this->value_.size() - this->value_read_offset_ < max_offset) {
          //  Last message in the chain
          response.attr_value.len = this->value_.size() - this->value_read_offset_;
          response.attr_value.offset = this->value_read_offset_;
          memcpy(response.attr_value.value, this->value_.data() + response.attr_value.offset, response.attr_value.len);
          this->value_read_offset_ = 0;
        } else {
          response.attr_value.len = max_offset;
          response.attr_value.offset = this->value_read_offset_;
          memcpy(response.attr_value.value, this->value_.data() + response.attr_value.offset, response.attr_value.len);
          this->value_read_offset_ += max_offset;
        }
      } else {
        response.attr_value.offset = 0;
        if (this->value_.size() + 1 > max_offset) {
          response.attr_value.len = max_offset;
          this->value_read_offset_ = max_offset;
        } else {
          response.attr_value.len = this->value_.size();
        }
        memcpy(response.attr_value.value, this->value_.data(), response.attr_value.len);
      }

      response.attr_value.handle = this->handle_;
      response.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;

      esp_err_t err =
          esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &response);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gatts_send_response failed: %d", err);
      }
      break;
    }
    case ESP_GATTS_WRITE_EVT: {
      if (this->handle_ != param->write.handle)
        return;

      if (param->write.is_prep) {
        this->value_.insert(this->value_.end(), param->write.value, param->write.value + param->write.len);
        this->write_event_ = true;
      } else {
        this->set_value(param->write.value, param->write.len);
      }

      if (param->write.need_rsp) {
        esp_gatt_rsp_t response;

        response.attr_value.len = param->write.len;
        response.attr_value.handle = this->handle_;
        response.attr_value.offset = param->write.offset;
        response.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
        memcpy(response.attr_value.value, param->write.value, param->write.len);

        esp_err_t err =
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, &response);

        if (err != ESP_OK) {
          ESP_LOGE(TAG, "esp_ble_gatts_send_response failed: %d", err);
        }
      }

      if (!param->write.is_prep) {
        this->on_write_(this->value_);
      }

      break;
    }

    case ESP_GATTS_EXEC_WRITE_EVT: {
      if (!this->write_event_)
        break;
      this->write_event_ = false;
      if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
        this->on_write_(this->value_);
      }
      esp_err_t err =
          esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, nullptr);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gatts_send_response failed: %d", err);
      }
      break;
    }
    default:
      break;
  }

  for (auto *descriptor : this->descriptors_) {
    descriptor->gatts_event_handler(event, gatts_if, param);
  }
}

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
