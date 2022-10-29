#include "ble_client_base.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace esp32_ble_client {

static const char *const TAG = "esp32_ble_client";

void BLEClientBase::setup() {
  auto ret = esp_ble_gattc_app_register(this->app_id);
  if (ret) {
    ESP_LOGE(TAG, "gattc app register failed. app_id=%d code=%d", this->app_id, ret);
    this->mark_failed();
  }
  this->set_state(espbt::ClientState::IDLE);
}

void BLEClientBase::loop() {
  if (this->state_ == espbt::ClientState::DISCOVERED) {
    this->connect();
  }
}

float BLEClientBase::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

bool BLEClientBase::parse_device(const espbt::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;
  if (this->state_ != espbt::ClientState::IDLE && this->state_ != espbt::ClientState::SEARCHING)
    return false;

  ESP_LOGD(TAG, "Found device at MAC address [%s]", device.address_str().c_str());
  this->set_state(espbt::ClientState::DISCOVERED);

  auto addr = device.address_uint64();
  this->remote_bda_[0] = (addr >> 40) & 0xFF;
  this->remote_bda_[1] = (addr >> 32) & 0xFF;
  this->remote_bda_[2] = (addr >> 24) & 0xFF;
  this->remote_bda_[3] = (addr >> 16) & 0xFF;
  this->remote_bda_[4] = (addr >> 8) & 0xFF;
  this->remote_bda_[5] = (addr >> 0) & 0xFF;
  this->remote_addr_type_ = device.get_address_type();
  return true;
}

std::string BLEClientBase::address_str() const {
  return str_snprintf("%02x:%02x:%02x:%02x:%02x:%02x", 17, (uint8_t)(this->address_ >> 40) & 0xff,
                      (uint8_t)(this->address_ >> 32) & 0xff, (uint8_t)(this->address_ >> 24) & 0xff,
                      (uint8_t)(this->address_ >> 16) & 0xff, (uint8_t)(this->address_ >> 8) & 0xff,
                      (uint8_t)(this->address_ >> 0) & 0xff);
}

void BLEClientBase::connect() {
  ESP_LOGI(TAG, "Attempting BLE connection to %s", this->address_str().c_str());
  auto ret = esp_ble_gattc_open(this->gattc_if_, this->remote_bda_, this->remote_addr_type_, true);
  if (ret) {
    ESP_LOGW(TAG, "esp_ble_gattc_open error, address=%s status=%d", this->address_str().c_str(), ret);
    this->set_state(espbt::ClientState::IDLE);
  } else {
    this->set_state(espbt::ClientState::CONNECTING);
  }
}

void BLEClientBase::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t esp_gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
  if (event == ESP_GATTC_REG_EVT && this->app_id != param->reg.app_id)
    return;
  if (event != ESP_GATTC_REG_EVT && esp_gattc_if != ESP_GATT_IF_NONE && esp_gattc_if != this->gattc_if_)
    return;

  switch (event) {
    case ESP_GATTC_REG_EVT: {
      if (param->reg.status == ESP_GATT_OK) {
        ESP_LOGV(TAG, "gattc registered app id %d", this->app_id);
        this->gattc_if_ = esp_gattc_if;
      } else {
        ESP_LOGE(TAG, "gattc app registration failed id=%d code=%d", param->reg.app_id, param->reg.status);
      }
      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      ESP_LOGV(TAG, "[%s] ESP_GATTC_OPEN_EVT", this->address_str().c_str());
      this->conn_id_ = param->open.conn_id;
      if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
        ESP_LOGW(TAG, "connect to %s failed, status=%d", this->address_str().c_str(), param->open.status);
        this->set_state(espbt::ClientState::IDLE);
        break;
      }
      break;
    }
    case ESP_GATTC_CONNECT_EVT: {
      ESP_LOGV(TAG, "[%s] ESP_GATTC_CONNECT_EVT", this->address_str().c_str());
      if (this->conn_id_ != param->connect.conn_id) {
        ESP_LOGD(TAG, "[%s] Unexpected conn_id in CONNECT_EVT: param conn=%d, open conn=%d",
                 this->address_str().c_str(), param->connect.conn_id, this->conn_id_);
      }
      auto ret = esp_ble_gattc_send_mtu_req(this->gattc_if_, param->connect.conn_id);
      if (ret) {
        ESP_LOGW(TAG, "esp_ble_gattc_send_mtu_req failed, status=%x", ret);
      }
      break;
    }
    case ESP_GATTC_CFG_MTU_EVT: {
      if (param->cfg_mtu.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "cfg_mtu to %s failed, mtu %d, status %d", this->address_str().c_str(), param->cfg_mtu.mtu,
                 param->cfg_mtu.status);
        this->set_state(espbt::ClientState::IDLE);
        break;
      }
      ESP_LOGV(TAG, "cfg_mtu status %d, mtu %d", param->cfg_mtu.status, param->cfg_mtu.mtu);
      this->mtu_ = param->cfg_mtu.mtu;
      esp_ble_gattc_search_service(esp_gattc_if, param->cfg_mtu.conn_id, nullptr);
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      if (memcmp(param->disconnect.remote_bda, this->remote_bda_, 6) != 0) {
        return;
      }
      ESP_LOGV(TAG, "[%s] ESP_GATTC_DISCONNECT_EVT, reason %d", this->address_str().c_str(), param->disconnect.reason);
      for (auto &svc : this->services_)
        delete svc;  // NOLINT(cppcoreguidelines-owning-memory)
      this->services_.clear();
      this->set_state(espbt::ClientState::IDLE);
      break;
    }
    case ESP_GATTC_SEARCH_RES_EVT: {
      BLEService *ble_service = new BLEService();  // NOLINT(cppcoreguidelines-owning-memory)
      ble_service->uuid = espbt::ESPBTUUID::from_uuid(param->search_res.srvc_id.uuid);
      ble_service->start_handle = param->search_res.start_handle;
      ble_service->end_handle = param->search_res.end_handle;
      ble_service->client = this;
      this->services_.push_back(ble_service);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGV(TAG, "[%s] ESP_GATTC_SEARCH_CMPL_EVT", this->address_str().c_str());
      for (auto &svc : this->services_) {
        ESP_LOGI(TAG, "Service UUID: %s", svc->uuid.to_string().c_str());
        ESP_LOGI(TAG, "  start_handle: 0x%x  end_handle: 0x%x", svc->start_handle, svc->end_handle);
        svc->parse_characteristics();
      }
      this->set_state(espbt::ClientState::CONNECTED);
      this->state_ = espbt::ClientState::ESTABLISHED;
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      auto *descr = this->get_config_descriptor(param->reg_for_notify.handle);
      if (descr == nullptr) {
        ESP_LOGW(TAG, "No descriptor found for notify of handle 0x%x", param->reg_for_notify.handle);
        break;
      }
      if (descr->uuid.get_uuid().len != ESP_UUID_LEN_16 ||
          descr->uuid.get_uuid().uuid.uuid16 != ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
        ESP_LOGW(TAG, "Handle 0x%x (uuid %s) is not a client config char uuid", param->reg_for_notify.handle,
                 descr->uuid.to_string().c_str());
        break;
      }
      uint16_t notify_en = 1;
      auto status =
          esp_ble_gattc_write_char_descr(this->gattc_if_, this->conn_id_, descr->handle, sizeof(notify_en),
                                         (uint8_t *) &notify_en, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_write_char_descr error, status=%d", status);
      }
      break;
    }

    default:
      break;
  }
}

void BLEClientBase::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    // This event is sent by the server when it requests security
    case ESP_GAP_BLE_SEC_REQ_EVT:
      ESP_LOGV(TAG, "ESP_GAP_BLE_SEC_REQ_EVT %x", event);
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;
    // This event is sent once authentication has completed
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      esp_bd_addr_t bd_addr;
      memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
      ESP_LOGI(TAG, "auth complete. remote BD_ADDR: %s", format_hex(bd_addr, 6).c_str());
      if (!param->ble_security.auth_cmpl.success) {
        ESP_LOGE(TAG, "auth fail reason = 0x%x", param->ble_security.auth_cmpl.fail_reason);
      } else {
        ESP_LOGV(TAG, "auth success. address type = %d auth mode = %d", param->ble_security.auth_cmpl.addr_type,
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
    case 0x7:  // uint24.
      if (length > 3) {
        return (float) encode_uint24(value[1], value[2], value[3]);
      }
    case 0x8:  // uint32.
      if (length > 4) {
        return (float) encode_uint32(value[1], value[2], value[3], value[4]);
      }
    case 0xC:  // int8.
      return (float) ((int8_t) value[1]);
    case 0xD:  // int12.
    case 0xE:  // int16.
      if (length > 2) {
        return (float) ((int16_t)(value[1] << 8) + (int16_t) value[2]);
      }
    case 0xF:  // int24.
      if (length > 3) {
        return (float) ((int32_t)(value[1] << 16) + (int32_t)(value[2] << 8) + (int32_t)(value[3]));
      }
    case 0x10:  // int32.
      if (length > 4) {
        return (float) ((int32_t)(value[1] << 24) + (int32_t)(value[2] << 16) + (int32_t)(value[3] << 8) +
                        (int32_t)(value[4]));
      }
  }
  ESP_LOGW(TAG, "Cannot parse characteristic value of type 0x%x length %d", value[0], length);
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
    for (auto &desc : chr->descriptors) {
      if (desc->uuid == espbt::ESPBTUUID::from_uint16(0x2902))
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
    for (auto *chr : svc->characteristics) {
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
