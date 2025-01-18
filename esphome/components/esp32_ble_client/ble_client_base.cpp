#include "ble_client_base.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_client {

static const char *const TAG = "esp32_ble_client";
static const esp_bt_uuid_t NOTIFY_DESC_UUID = {
    .len = ESP_UUID_LEN_16,
    .uuid =
        {
            .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
        },
};

void BLEClientBase::setup() {
  static uint8_t connection_index = 0;
  this->connection_index_ = connection_index++;
}

void BLEClientBase::loop() {
  if (!esp32_ble::global_ble->is_active()) {
    this->set_state(espbt::ClientState::INIT);
    return;
  }
  if (this->state_ == espbt::ClientState::INIT) {
    auto ret = esp_ble_gattc_app_register(this->app_id);
    if (ret) {
      ESP_LOGE(TAG, "gattc app register failed. app_id=%d code=%d", this->app_id, ret);
      this->mark_failed();
    }
    this->set_state(espbt::ClientState::IDLE);
  }
  // READY_TO_CONNECT means we have discovered the device
  // and the scanner has been stopped by the tracker.
  if (this->state_ == espbt::ClientState::READY_TO_CONNECT) {
    this->connect();
  }
}

float BLEClientBase::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void BLEClientBase::dump_config() {
  ESP_LOGCONFIG(TAG, "  Address: %s", this->address_str().c_str());
  ESP_LOGCONFIG(TAG, "  Auto-Connect: %s", TRUEFALSE(this->auto_connect_));
  std::string state_name;
  switch (this->state()) {
    case espbt::ClientState::INIT:
      state_name = "INIT";
      break;
    case espbt::ClientState::DISCONNECTING:
      state_name = "DISCONNECTING";
      break;
    case espbt::ClientState::IDLE:
      state_name = "IDLE";
      break;
    case espbt::ClientState::SEARCHING:
      state_name = "SEARCHING";
      break;
    case espbt::ClientState::DISCOVERED:
      state_name = "DISCOVERED";
      break;
    case espbt::ClientState::READY_TO_CONNECT:
      state_name = "READY_TO_CONNECT";
      break;
    case espbt::ClientState::CONNECTING:
      state_name = "CONNECTING";
      break;
    case espbt::ClientState::CONNECTED:
      state_name = "CONNECTED";
      break;
    case espbt::ClientState::ESTABLISHED:
      state_name = "ESTABLISHED";
      break;
    default:
      state_name = "UNKNOWN_STATE";
      break;
  }
  ESP_LOGCONFIG(TAG, "  State: %s", state_name.c_str());
  if (this->status_ == ESP_GATT_NO_RESOURCES) {
    ESP_LOGE(TAG, "  Failed due to no resources. Try to reduce number of BLE clients in config.");
  } else if (this->status_ != ESP_GATT_OK) {
    ESP_LOGW(TAG, "  Failed due to error code %d", this->status_);
  }
}

bool BLEClientBase::parse_device(const espbt::ESPBTDevice &device) {
  if (!this->auto_connect_)
    return false;
  if (this->address_ == 0 || device.address_uint64() != this->address_)
    return false;
  if (this->state_ != espbt::ClientState::IDLE && this->state_ != espbt::ClientState::SEARCHING)
    return false;

  this->log_event_("Found device");
  if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG)
    esp32_ble_tracker::global_esp32_ble_tracker->print_bt_device_info(device);

  this->set_state(espbt::ClientState::DISCOVERED);
  this->set_address(device.address_uint64());
  this->remote_addr_type_ = device.get_address_type();
  return true;
}

void BLEClientBase::connect() {
  ESP_LOGI(TAG, "[%d] [%s] 0x%02x Attempting BLE connection", this->connection_index_, this->address_str_.c_str(),
           this->remote_addr_type_);
  this->paired_ = false;
  auto ret = esp_ble_gattc_open(this->gattc_if_, this->remote_bda_, this->remote_addr_type_, true);
  if (ret) {
    ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_open error, status=%d", this->connection_index_, this->address_str_.c_str(),
             ret);
    this->set_state(espbt::ClientState::IDLE);
  } else {
    this->set_state(espbt::ClientState::CONNECTING);
  }
}

esp_err_t BLEClientBase::pair() { return esp_ble_set_encryption(this->remote_bda_, ESP_BLE_SEC_ENCRYPT); }

void BLEClientBase::disconnect() {
  if (this->state_ == espbt::ClientState::IDLE || this->state_ == espbt::ClientState::DISCONNECTING)
    return;
  ESP_LOGI(TAG, "[%d] [%s] Disconnecting.", this->connection_index_, this->address_str_.c_str());
  auto err = esp_ble_gattc_close(this->gattc_if_, this->conn_id_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_close error, err=%d", this->connection_index_, this->address_str_.c_str(),
             err);
  }

  if (this->state_ == espbt::ClientState::SEARCHING || this->state_ == espbt::ClientState::READY_TO_CONNECT ||
      this->state_ == espbt::ClientState::DISCOVERED) {
    this->set_address(0);
    this->set_state(espbt::ClientState::IDLE);
  } else {
    this->set_state(espbt::ClientState::DISCONNECTING);
  }
}

void BLEClientBase::release_services() {
  for (auto &svc : this->services_)
    delete svc;  // NOLINT(cppcoreguidelines-owning-memory)
  this->services_.clear();
#ifndef CONFIG_BT_GATTC_CACHE_NVS_FLASH
  esp_ble_gattc_cache_clean(this->remote_bda_);
#endif
}

void BLEClientBase::log_event_(const char *name) {
  ESP_LOGD(TAG, "[%d] [%s] %s", this->connection_index_, this->address_str_.c_str(), name);
}

bool BLEClientBase::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t esp_gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
  if (event == ESP_GATTC_REG_EVT && this->app_id != param->reg.app_id)
    return false;
  if (event != ESP_GATTC_REG_EVT && esp_gattc_if != ESP_GATT_IF_NONE && esp_gattc_if != this->gattc_if_)
    return false;

  ESP_LOGV(TAG, "[%d] [%s] gattc_event_handler: event=%d gattc_if=%d", this->connection_index_,
           this->address_str_.c_str(), event, esp_gattc_if);

  switch (event) {
    case ESP_GATTC_REG_EVT: {
      if (param->reg.status == ESP_GATT_OK) {
        ESP_LOGV(TAG, "[%d] [%s] gattc registered app id %d", this->connection_index_, this->address_str_.c_str(),
                 this->app_id);
        this->gattc_if_ = esp_gattc_if;
      } else {
        ESP_LOGE(TAG, "[%d] [%s] gattc app registration failed id=%d code=%d", this->connection_index_,
                 this->address_str_.c_str(), param->reg.app_id, param->reg.status);
        this->status_ = param->reg.status;
        this->mark_failed();
      }
      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      if (!this->check_addr(param->open.remote_bda))
        return false;
      this->log_event_("ESP_GATTC_OPEN_EVT");
      this->conn_id_ = param->open.conn_id;
      this->service_count_ = 0;
      if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
        ESP_LOGW(TAG, "[%d] [%s] Connection failed, status=%d", this->connection_index_, this->address_str_.c_str(),
                 param->open.status);
        this->set_state(espbt::ClientState::IDLE);
        break;
      }
      auto ret = esp_ble_gattc_send_mtu_req(this->gattc_if_, param->open.conn_id);
      if (ret) {
        ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_send_mtu_req failed, status=%x", this->connection_index_,
                 this->address_str_.c_str(), ret);
      }
      this->set_state(espbt::ClientState::CONNECTED);
      if (this->connection_type_ == espbt::ConnectionType::V3_WITH_CACHE) {
        ESP_LOGI(TAG, "[%d] [%s] Connected", this->connection_index_, this->address_str_.c_str());
        // only set our state, subclients might have more stuff to do yet.
        this->state_ = espbt::ClientState::ESTABLISHED;
        break;
      }
      esp_ble_gattc_search_service(esp_gattc_if, param->cfg_mtu.conn_id, nullptr);
      break;
    }
    case ESP_GATTC_CONNECT_EVT: {
      if (!this->check_addr(param->connect.remote_bda))
        return false;
      this->log_event_("ESP_GATTC_CONNECT_EVT");
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      if (!this->check_addr(param->disconnect.remote_bda))
        return false;
      ESP_LOGD(TAG, "[%d] [%s] ESP_GATTC_DISCONNECT_EVT, reason %d", this->connection_index_,
               this->address_str_.c_str(), param->disconnect.reason);
      this->release_services();
      this->set_state(espbt::ClientState::IDLE);
      break;
    }

    case ESP_GATTC_CFG_MTU_EVT: {
      if (this->conn_id_ != param->cfg_mtu.conn_id)
        return false;
      if (param->cfg_mtu.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] cfg_mtu failed, mtu %d, status %d", this->connection_index_,
                 this->address_str_.c_str(), param->cfg_mtu.mtu, param->cfg_mtu.status);
        // No state change required here - disconnect event will follow if needed.
        break;
      }
      ESP_LOGD(TAG, "[%d] [%s] cfg_mtu status %d, mtu %d", this->connection_index_, this->address_str_.c_str(),
               param->cfg_mtu.status, param->cfg_mtu.mtu);
      this->mtu_ = param->cfg_mtu.mtu;
      break;
    }
    case ESP_GATTC_CLOSE_EVT: {
      if (this->conn_id_ != param->close.conn_id)
        return false;
      this->log_event_("ESP_GATTC_CLOSE_EVT");
      this->release_services();
      this->set_state(espbt::ClientState::IDLE);
      break;
    }
    case ESP_GATTC_SEARCH_RES_EVT: {
      if (this->conn_id_ != param->search_res.conn_id)
        return false;
      this->service_count_++;
      if (this->connection_type_ == espbt::ConnectionType::V3_WITHOUT_CACHE) {
        // V3 clients don't need services initialized since
        // they only request by handle after receiving the services.
        break;
      }
      BLEService *ble_service = new BLEService();  // NOLINT(cppcoreguidelines-owning-memory)
      ble_service->uuid = espbt::ESPBTUUID::from_uuid(param->search_res.srvc_id.uuid);
      ble_service->start_handle = param->search_res.start_handle;
      ble_service->end_handle = param->search_res.end_handle;
      ble_service->client = this;
      this->services_.push_back(ble_service);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (this->conn_id_ != param->search_cmpl.conn_id)
        return false;
      this->log_event_("ESP_GATTC_SEARCH_CMPL_EVT");
      for (auto &svc : this->services_) {
        ESP_LOGV(TAG, "[%d] [%s] Service UUID: %s", this->connection_index_, this->address_str_.c_str(),
                 svc->uuid.to_string().c_str());
        ESP_LOGV(TAG, "[%d] [%s]  start_handle: 0x%x  end_handle: 0x%x", this->connection_index_,
                 this->address_str_.c_str(), svc->start_handle, svc->end_handle);
      }
      ESP_LOGI(TAG, "[%d] [%s] Connected", this->connection_index_, this->address_str_.c_str());
      this->state_ = espbt::ClientState::ESTABLISHED;
      break;
    }
    case ESP_GATTC_READ_DESCR_EVT: {
      if (this->conn_id_ != param->write.conn_id)
        return false;
      this->log_event_("ESP_GATTC_READ_DESCR_EVT");
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (this->conn_id_ != param->write.conn_id)
        return false;
      this->log_event_("ESP_GATTC_WRITE_DESCR_EVT");
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (this->conn_id_ != param->write.conn_id)
        return false;
      this->log_event_("ESP_GATTC_WRITE_CHAR_EVT");
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (this->conn_id_ != param->read.conn_id)
        return false;
      this->log_event_("ESP_GATTC_READ_CHAR_EVT");
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (this->conn_id_ != param->notify.conn_id)
        return false;
      this->log_event_("ESP_GATTC_NOTIFY_EVT");
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->log_event_("ESP_GATTC_REG_FOR_NOTIFY_EVT");
      if (this->connection_type_ == espbt::ConnectionType::V3_WITH_CACHE ||
          this->connection_type_ == espbt::ConnectionType::V3_WITHOUT_CACHE) {
        // Client is responsible for flipping the descriptor value
        // when using the cache
        break;
      }
      esp_gattc_descr_elem_t desc_result;
      uint16_t count = 1;
      esp_gatt_status_t descr_status = esp_ble_gattc_get_descr_by_char_handle(
          this->gattc_if_, this->conn_id_, param->reg_for_notify.handle, NOTIFY_DESC_UUID, &desc_result, &count);
      if (descr_status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_get_descr_by_char_handle error, status=%d", this->connection_index_,
                 this->address_str_.c_str(), descr_status);
        break;
      }
      esp_gattc_char_elem_t char_result;
      esp_gatt_status_t char_status =
          esp_ble_gattc_get_all_char(this->gattc_if_, this->conn_id_, param->reg_for_notify.handle,
                                     param->reg_for_notify.handle, &char_result, &count, 0);
      if (char_status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_get_all_char error, status=%d", this->connection_index_,
                 this->address_str_.c_str(), char_status);
        break;
      }

      /*
        1 = notify
        2 = indicate
      */
      uint16_t notify_en = char_result.properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY ? 1 : 2;
      esp_err_t status =
          esp_ble_gattc_write_char_descr(this->gattc_if_, this->conn_id_, desc_result.handle, sizeof(notify_en),
                                         (uint8_t *) &notify_en, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
      ESP_LOGD(TAG, "Wrote notify descriptor %d, properties=%d", notify_en, char_result.properties);
      if (status) {
        ESP_LOGW(TAG, "[%d] [%s] esp_ble_gattc_write_char_descr error, status=%d", this->connection_index_,
                 this->address_str_.c_str(), status);
      }
      break;
    }

    default:
      // ideally would check all other events for matching conn_id
      ESP_LOGD(TAG, "[%d] [%s] Event %d", this->connection_index_, this->address_str_.c_str(), event);
      break;
  }
  return true;
}

// clients can't call defer() directly since it's protected.
void BLEClientBase::run_later(std::function<void()> &&f) {  // NOLINT
  this->defer(std::move(f));
}

void BLEClientBase::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    // This event is sent by the server when it requests security
    case ESP_GAP_BLE_SEC_REQ_EVT:
      if (!this->check_addr(param->ble_security.auth_cmpl.bd_addr))
        return;
      ESP_LOGV(TAG, "[%d] [%s] ESP_GAP_BLE_SEC_REQ_EVT %x", this->connection_index_, this->address_str_.c_str(), event);
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;
    // This event is sent once authentication has completed
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      if (!this->check_addr(param->ble_security.auth_cmpl.bd_addr))
        return;
      esp_bd_addr_t bd_addr;
      memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
      ESP_LOGI(TAG, "[%d] [%s] auth complete. remote BD_ADDR: %s", this->connection_index_, this->address_str_.c_str(),
               format_hex(bd_addr, 6).c_str());
      if (!param->ble_security.auth_cmpl.success) {
        ESP_LOGE(TAG, "[%d] [%s] auth fail reason = 0x%x", this->connection_index_, this->address_str_.c_str(),
                 param->ble_security.auth_cmpl.fail_reason);
      } else {
        this->paired_ = true;
        ESP_LOGD(TAG, "[%d] [%s] auth success. address type = %d auth mode = %d", this->connection_index_,
                 this->address_str_.c_str(), param->ble_security.auth_cmpl.addr_type,
                 param->ble_security.auth_cmpl.auth_mode);
      }
      break;

    // There are other events we'll want to implement at some point to support things like pass key
    // https://github.com/espressif/esp-idf/blob/cba69dd088344ed9d26739f04736ae7a37541b3a/examples/bluetooth/bluedroid/ble/gatt_security_client/tutorial/Gatt_Security_Client_Example_Walkthrough.md
    default:
      break;
  }
}

// Parse GATT values into a float for a sensor.
// Ref: https://www.bluetooth.com/specifications/assigned-numbers/format-types/
float BLEClientBase::parse_char_value(uint8_t *value, uint16_t length) {
  // A length of one means a single octet value.
  if (length == 0)
    return 0;
  if (length == 1)
    return (float) ((uint8_t) value[0]);

  switch (value[0]) {
    case 0x1:  // boolean.
    case 0x2:  // 2bit.
    case 0x3:  // nibble.
    case 0x4:  // uint8.
      return (float) ((uint8_t) value[1]);
    case 0x5:  // uint12.
    case 0x6:  // uint16.
      if (length > 2) {
        return (float) encode_uint16(value[1], value[2]);
      }
      // fall through
    case 0x7:  // uint24.
      if (length > 3) {
        return (float) encode_uint24(value[1], value[2], value[3]);
      }
      // fall through
    case 0x8:  // uint32.
      if (length > 4) {
        return (float) encode_uint32(value[1], value[2], value[3], value[4]);
      }
      // fall through
    case 0xC:  // int8.
      return (float) ((int8_t) value[1]);
    case 0xD:  // int12.
    case 0xE:  // int16.
      if (length > 2) {
        return (float) ((int16_t) (value[1] << 8) + (int16_t) value[2]);
      }
      // fall through
    case 0xF:  // int24.
      if (length > 3) {
        return (float) ((int32_t) (value[1] << 16) + (int32_t) (value[2] << 8) + (int32_t) (value[3]));
      }
      // fall through
    case 0x10:  // int32.
      if (length > 4) {
        return (float) ((int32_t) (value[1] << 24) + (int32_t) (value[2] << 16) + (int32_t) (value[3] << 8) +
                        (int32_t) (value[4]));
      }
  }
  ESP_LOGW(TAG, "[%d] [%s] Cannot parse characteristic value of type 0x%x length %d", this->connection_index_,
           this->address_str_.c_str(), value[0], length);
  return NAN;
}

BLEService *BLEClientBase::get_service(espbt::ESPBTUUID uuid) {
  for (auto *svc : this->services_) {
    if (svc->uuid == uuid)
      return svc;
  }
  return nullptr;
}

BLEService *BLEClientBase::get_service(uint16_t uuid) { return this->get_service(espbt::ESPBTUUID::from_uint16(uuid)); }

BLECharacteristic *BLEClientBase::get_characteristic(espbt::ESPBTUUID service, espbt::ESPBTUUID chr) {
  auto *svc = this->get_service(service);
  if (svc == nullptr)
    return nullptr;
  return svc->get_characteristic(chr);
}

BLECharacteristic *BLEClientBase::get_characteristic(uint16_t service, uint16_t chr) {
  return this->get_characteristic(espbt::ESPBTUUID::from_uint16(service), espbt::ESPBTUUID::from_uint16(chr));
}

BLECharacteristic *BLEClientBase::get_characteristic(uint16_t handle) {
  for (auto *svc : this->services_) {
    if (!svc->parsed)
      svc->parse_characteristics();
    for (auto *chr : svc->characteristics) {
      if (chr->handle == handle)
        return chr;
    }
  }
  return nullptr;
}

BLEDescriptor *BLEClientBase::get_config_descriptor(uint16_t handle) {
  auto *chr = this->get_characteristic(handle);
  if (chr != nullptr) {
    if (!chr->parsed)
      chr->parse_descriptors();
    for (auto &desc : chr->descriptors) {
      if (desc->uuid.get_uuid().uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG)
        return desc;
    }
  }
  return nullptr;
}

BLEDescriptor *BLEClientBase::get_descriptor(espbt::ESPBTUUID service, espbt::ESPBTUUID chr, espbt::ESPBTUUID descr) {
  auto *svc = this->get_service(service);
  if (svc == nullptr)
    return nullptr;
  auto *ch = svc->get_characteristic(chr);
  if (ch == nullptr)
    return nullptr;
  return ch->get_descriptor(descr);
}

BLEDescriptor *BLEClientBase::get_descriptor(uint16_t service, uint16_t chr, uint16_t descr) {
  return this->get_descriptor(espbt::ESPBTUUID::from_uint16(service), espbt::ESPBTUUID::from_uint16(chr),
                              espbt::ESPBTUUID::from_uint16(descr));
}

BLEDescriptor *BLEClientBase::get_descriptor(uint16_t handle) {
  for (auto *svc : this->services_) {
    if (!svc->parsed)
      svc->parse_characteristics();
    for (auto *chr : svc->characteristics) {
      if (!chr->parsed)
        chr->parse_descriptors();
      for (auto *desc : chr->descriptors) {
        if (desc->handle == handle)
          return desc;
      }
    }
  }
  return nullptr;
}

}  // namespace esp32_ble_client
}  // namespace esphome

#endif  // USE_ESP32
