#include "ble_characteristic.h"
#include "ble_server.h"
#include "ble_service.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_server {

static const char *const TAG = "esp32_ble_server.characteristic";

BLECharacteristic::~BLECharacteristic() {
  for (auto *descriptor : this->descriptors_) {
    delete descriptor;  // NOLINT(cppcoreguidelines-owning-memory)
  }
  vSemaphoreDelete(this->set_value_lock_);
}

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

void BLECharacteristic::set_value(ByteBuffer buffer) { this->set_value(buffer.get_data()); }

void BLECharacteristic::set_value(std::vector<uint8_t> &&buffer) {
  xSemaphoreTake(this->set_value_lock_, 0L);
  this->value_ = std::move(buffer);
  xSemaphoreGive(this->set_value_lock_);
}

void BLECharacteristic::set_value(std::initializer_list<uint8_t> data) {
  this->set_value(std::vector<uint8_t>(data));  // Delegate to move overload
}

void BLECharacteristic::set_value(const std::string &buffer) {
  this->set_value(std::vector<uint8_t>(buffer.begin(), buffer.end()));  // Delegate to move overload
}

void BLECharacteristic::notify() {
  if (this->service_ == nullptr || this->service_->get_server() == nullptr ||
      this->service_->get_server()->get_connected_client_count() == 0)
    return;

  const uint16_t *clients = this->service_->get_server()->get_clients();
  uint8_t client_count = this->service_->get_server()->get_client_count();

  for (uint8_t i = 0; i < client_count; i++) {
    uint16_t client = clients[i];
    size_t length = this->value_.size();
    // Find the client in the list of clients to notify
    auto *entry = this->find_client_in_notify_list_(client);
    if (entry == nullptr)
      continue;
    bool require_ack = entry->indicate;
    // TODO: Remove this block when INDICATE acknowledgment is supported
    if (require_ack) {
      ESP_LOGW(TAG, "INDICATE acknowledgment is not yet supported (i.e. it works as a NOTIFY)");
      require_ack = false;
    }
    esp_err_t err = esp_ble_gatts_send_indicate(this->service_->get_server()->get_gatts_if(), client, this->handle_,
                                                length, this->value_.data(), require_ack);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_ble_gatts_send_indicate failed %d", err);
      return;
    }
  }
}

void BLECharacteristic::add_descriptor(BLEDescriptor *descriptor) {
  // If the descriptor is the CCCD descriptor, listen to its write event to know if the client wants to be notified
  if (descriptor->get_uuid() == ESPBTUUID::from_uint16(ESP_GATT_UUID_CHAR_CLIENT_CONFIG)) {
    descriptor->on_write([this](std::span<const uint8_t> value, uint16_t conn_id) {
      if (value.size() != 2)
        return;
      uint16_t cccd = encode_uint16(value[1], value[0]);
      bool notify = (cccd & 1) != 0;
      bool indicate = (cccd & 2) != 0;
      // Remove existing entry if present
      this->remove_client_from_notify_list_(conn_id);
      // Add new entry if needed
      if (notify || indicate) {
        this->clients_to_notify_.push_back({conn_id, indicate});
      }
    });
  }
  this->descriptors_.push_back(descriptor);
}

void BLECharacteristic::remove_descriptor(BLEDescriptor *descriptor) {
  this->descriptors_.erase(std::remove(this->descriptors_.begin(), this->descriptors_.end(), descriptor),
                           this->descriptors_.end());
}

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

  for (auto *descriptor : this->descriptors_) {
    if (!descriptor->is_created())
      return false;
  }
  // All descriptors are created if we reach here
  this->state_ = CREATED;
  return true;
}

bool BLECharacteristic::is_failed() {
  if (this->state_ == FAILED)
    return true;

  for (auto *descriptor : this->descriptors_) {
    if (descriptor->is_failed()) {
      this->state_ = FAILED;
      return true;
    }
  }
  return false;
}

void BLECharacteristic::set_property_bit_(esp_gatt_char_prop_t bit, bool value) {
  if (value) {
    this->properties_ = (esp_gatt_char_prop_t) (this->properties_ | bit);
  } else {
    this->properties_ = (esp_gatt_char_prop_t) (this->properties_ & ~bit);
  }
}

void BLECharacteristic::set_broadcast_property(bool value) {
  this->set_property_bit_(ESP_GATT_CHAR_PROP_BIT_BROADCAST, value);
}
void BLECharacteristic::set_indicate_property(bool value) {
  this->set_property_bit_(ESP_GATT_CHAR_PROP_BIT_INDICATE, value);
}
void BLECharacteristic::set_notify_property(bool value) {
  this->set_property_bit_(ESP_GATT_CHAR_PROP_BIT_NOTIFY, value);
}
void BLECharacteristic::set_read_property(bool value) { this->set_property_bit_(ESP_GATT_CHAR_PROP_BIT_READ, value); }
void BLECharacteristic::set_write_property(bool value) { this->set_property_bit_(ESP_GATT_CHAR_PROP_BIT_WRITE, value); }
void BLECharacteristic::set_write_no_response_property(bool value) {
  this->set_property_bit_(ESP_GATT_CHAR_PROP_BIT_WRITE_NR, value);
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

      if (this->on_read_callback_) {
        (*this->on_read_callback_)(param->read.conn_id);
      }

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
        break;

      if (param->write.is_prep) {
        this->value_.insert(this->value_.end(), param->write.value, param->write.value + param->write.len);
        this->write_event_ = true;
      } else {
        this->set_value(ByteBuffer::wrap(param->write.value, param->write.len));
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
        if (this->on_write_callback_) {
          (*this->on_write_callback_)(this->value_, param->write.conn_id);
        }
      }

      break;
    }

    case ESP_GATTS_EXEC_WRITE_EVT: {
      if (!this->write_event_)
        break;
      this->write_event_ = false;
      if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
        if (this->on_write_callback_) {
          (*this->on_write_callback_)(this->value_, param->exec_write.conn_id);
        }
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

void BLECharacteristic::remove_client_from_notify_list_(uint16_t conn_id) {
  // Since we typically have very few clients (often just 1), we can optimize
  // for the common case by swapping with the last element and popping
  for (size_t i = 0; i < this->clients_to_notify_.size(); i++) {
    if (this->clients_to_notify_[i].conn_id == conn_id) {
      // Swap with last element and pop (safe even when i is the last element)
      this->clients_to_notify_[i] = this->clients_to_notify_.back();
      this->clients_to_notify_.pop_back();
      return;
    }
  }
}

BLECharacteristic::ClientNotificationEntry *BLECharacteristic::find_client_in_notify_list_(uint16_t conn_id) {
  for (auto &entry : this->clients_to_notify_) {
    if (entry.conn_id == conn_id) {
      return &entry;
    }
  }
  return nullptr;
}

}  // namespace esp32_ble_server
}  // namespace esphome

#endif
