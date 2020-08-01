#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "ble_client.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ble_client {

static const char *TAG = "ble_client";

void BLEClient::setup() {
  auto ret = esp_ble_gattc_app_register(this->app_id_);
  if (ret) {
    ESP_LOGE(TAG, "gattc app register failed. app_id=%d code=%d", this->app_id_, ret);
    this->mark_failed();
  }
  this->set_states(espbt::ClientState::Idle);
}

void BLEClient::loop() {
  if (this->state_ == espbt::ClientState::Discovered) {
    this->connect();
  }
  for (auto *node : this->nodes_)
    node->loop();
}

void BLEClient::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Client:");
  ESP_LOGCONFIG(TAG, "  Address: %s", this->address_str().c_str());
}

bool BLEClient::parse_device(const espbt::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;
  if (this->state_ != espbt::ClientState::Idle)
    return false;

  ESP_LOGD(TAG, "Found device at MAC address [%s]", device.address_str().c_str());
  this->set_states(espbt::ClientState::Discovered);

  auto addr = device.address_uint64();
  this->remote_bda_[0] = (addr >> 40) & 0xFF;
  this->remote_bda_[1] = (addr >> 32) & 0xFF;
  this->remote_bda_[2] = (addr >> 24) & 0xFF;
  this->remote_bda_[3] = (addr >> 16) & 0xFF;
  this->remote_bda_[4] = (addr >> 8) & 0xFF;
  this->remote_bda_[5] = (addr >> 0) & 0xFF;
  return true;
}

std::string BLEClient::address_str() const {
  char buf[20];
  sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", (uint8_t)(this->address_ >> 40) & 0xff,
          (uint8_t)(this->address_ >> 32) & 0xff, (uint8_t)(this->address_ >> 24) & 0xff,
          (uint8_t)(this->address_ >> 16) & 0xff, (uint8_t)(this->address_ >> 8) & 0xff,
          (uint8_t)(this->address_ >> 0) & 0xff);
  std::string ret;
  ret = buf;
  return ret;
}

void BLEClient::connect() {
  ESP_LOGI(TAG, "Attempting BLE connection to %s", this->address_str().c_str());
  auto ret = esp_ble_gattc_open(this->gattc_if_, this->remote_bda_, BLE_ADDR_TYPE_PUBLIC, true);
  if (ret) {
    ESP_LOGW(TAG, "esp_ble_gattc_open error, address=%s status=%d", this->address_str().c_str(), ret);
    this->set_states(espbt::ClientState::Idle);
  } else {
    this->set_states(espbt::ClientState::Connecting);
  }
}

void BLEClient::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
  if (event == ESP_GATTC_REG_EVT && this->app_id_ != param->reg.app_id)
    return;
  if (event != ESP_GATTC_REG_EVT && gattc_if != ESP_GATT_IF_NONE && gattc_if != this->gattc_if_)
    return;

  bool all_established = this->all_nodes_established();

  switch (event) {
    case ESP_GATTC_REG_EVT: {
      if (param->reg.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "gattc registered app id %d", this->app_id_);
        this->gattc_if_ = gattc_if;
      } else {
        ESP_LOGE(TAG, "gattc app registration failed id=%d code=%d", param->reg.app_id, param->reg.status);
      }
      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      ESP_LOGI(TAG, "[%s] ESP_GATTC_OPEN_EVT", this->address_str().c_str());
      if (param->open.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "connect to %s failed, status=%d", this->address_str().c_str(), param->open.status);
        this->set_states(espbt::ClientState::Idle);
        break;
      }
      this->conn_id_ = param->open.conn_id;
      auto ret = esp_ble_gattc_send_mtu_req(this->gattc_if_, param->open.conn_id);
      if (ret) {
        ESP_LOGW(TAG, "esp_ble_gattc_send_mtu_req failed, status=%d", ret);
      }
      break;
    }
    case ESP_GATTC_CFG_MTU_EVT: {
      if (param->cfg_mtu.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "cfg_mtu to %s failed, status %d", this->address_str().c_str(), param->cfg_mtu.status);
        this->set_states(espbt::ClientState::Idle);
        break;
      }
      ESP_LOGV(TAG, "cfg_mtu status %d, mtu %d", param->cfg_mtu.status, param->cfg_mtu.mtu);
      esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, NULL);
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      if (memcmp(param->disconnect.remote_bda, this->remote_bda_, 6) != 0) {
        return;
      }
      ESP_LOGI(TAG, "[%s] ESP_GATTC_DISCONNECT_EVT", this->address_str().c_str());
      for (auto &svc : this->services_)
        delete svc;
      this->services_.clear();
      this->set_states(espbt::ClientState::Idle);
      break;
    }
    case ESP_GATTC_SEARCH_RES_EVT: {
      BLEService *ble_service = new BLEService();
      ble_service->uuid_ = espbt::ESPBTUUID::from_uuid(param->search_res.srvc_id.uuid);
      ble_service->start_handle_ = param->search_res.start_handle;
      ble_service->end_handle_ = param->search_res.end_handle;
      ble_service->client_ = this;
      this->services_.push_back(ble_service);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGV(TAG, "[%s] ESP_GATTC_SEARCH_CMPL_EVT", this->address_str().c_str());
      for (auto &svc : this->services_) {
        ESP_LOGI(TAG, "Service UUID: %s", svc->uuid_.to_string().c_str());
        ESP_LOGI(TAG, "  start_handle: 0x%x  end_handle: 0x%x", svc->start_handle_, svc->end_handle_);
        svc->parse_characteristics();
      }
      this->set_states(espbt::ClientState::Connected);
      this->state_ = espbt::ClientState::Established;
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      auto descr = this->get_config_descriptor(param->reg_for_notify.handle);
      if (descr == nullptr) {
        ESP_LOGW(TAG, "No descriptor found for notify of handle 0x%x", param->reg_for_notify.handle);
        break;
      }
      if (descr->uuid_.get_uuid().len != ESP_UUID_LEN_16 ||
          descr->uuid_.get_uuid().uuid.uuid16 != ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
        ESP_LOGW(TAG, "Handle 0x%x (uuid %s) is not a client config char uuid", param->reg_for_notify.handle,
                 descr->uuid_.to_string().c_str());
        break;
      }
      uint8_t notify_en = 1;
      auto status = esp_ble_gattc_write_char_descr(this->gattc_if_, this->conn_id_, descr->handle_, sizeof(notify_en),
                                                   &notify_en, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_write_char_descr error, status=%d", status);
      }
      break;
    }

    default:
      break;
  }
  for (auto *node : this->nodes_)
    node->gattc_event_handler(event, gattc_if, param);

  // Delete characteristics after clients have used them to save RAM.
  if (!all_established && this->all_nodes_established()) {
    for (auto &svc : this->services_)
      delete svc;
    this->services_.clear();
  }
}

// Parse GATT values into a float for a sensor.
// Ref: https://www.bluetooth.com/specifications/assigned-numbers/format-types/
float BLEClient::parse_char_value(uint8_t *value, uint16_t length) {
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
      return (float) ((uint16_t)(value[1] << 8) + (uint16_t) value[2]);
    case 0x7:  // uint24.
      return (float) ((uint32_t)(value[1] << 16) + (uint32_t)(value[2] << 8) + (uint32_t)(value[3]));
    case 0x8:  // uint32.
      return (float) ((uint32_t)(value[1] << 24) + (uint32_t)(value[2] << 16) + (uint32_t)(value[3] << 8) +
                      (uint32_t)(value[4]));
    case 0xC:  // int8.
      return (float) ((int8_t) value[1]);
    case 0xD:  // int12.
    case 0xE:  // int16.
      return (float) ((int16_t)(value[1] << 8) + (int16_t) value[2]);
    case 0xF:  // int24.
      return (float) ((int32_t)(value[1] << 16) + (int32_t)(value[2] << 8) + (int32_t)(value[3]));
    case 0x10:  // int32.
      return (float) ((int32_t)(value[1] << 24) + (int32_t)(value[2] << 16) + (int32_t)(value[3] << 8) +
                      (int32_t)(value[4]));
  }
  ESP_LOGW(TAG, "Cannot parse characteristic value of type 0x%x", value[0]);
  return NAN;
}

BLEService *BLEClient::get_service(espbt::ESPBTUUID uuid) {
  for (auto svc : this->services_)
    if (svc->uuid_ == uuid)
      return svc;
  return nullptr;
}

BLEService *BLEClient::get_service(uint16_t uuid) { return this->get_service(espbt::ESPBTUUID::from_uint16(uuid)); }

BLECharacteristic *BLEClient::get_characteristic(espbt::ESPBTUUID service, espbt::ESPBTUUID chr) {
  auto svc = this->get_service(service);
  if (svc == nullptr)
    return nullptr;
  return svc->get_characteristic(chr);
}

BLECharacteristic *BLEClient::get_characteristic(uint16_t service, uint16_t chr) {
  return this->get_characteristic(espbt::ESPBTUUID::from_uint16(service), espbt::ESPBTUUID::from_uint16(chr));
}

BLEDescriptor *BLEClient::get_config_descriptor(uint16_t handle) {
  for (auto &svc : this->services_)
    for (auto &chr : svc->characteristics_)
      if (chr->handle_ == handle)
        for (auto &desc : chr->descriptors_)
          if (desc->uuid_ == espbt::ESPBTUUID::from_uint16(0x2902))
            return desc;
  return nullptr;
}

BLECharacteristic *BLEService::get_characteristic(espbt::ESPBTUUID uuid) {
  for (auto &chr : this->characteristics_)
    if (chr->uuid_ == uuid)
      return chr;
  return nullptr;
}

BLECharacteristic *BLEService::get_characteristic(uint16_t uuid) {
  return this->get_characteristic(espbt::ESPBTUUID::from_uint16(uuid));
}

BLEDescriptor *BLEClient::get_descriptor(espbt::ESPBTUUID service, espbt::ESPBTUUID chr, espbt::ESPBTUUID descr) {
  auto svc = this->get_service(service);
  if (svc == nullptr)
    return nullptr;
  auto ch = svc->get_characteristic(chr);
  if (ch == nullptr)
    return nullptr;
  return ch->get_descriptor(descr);
}

BLEDescriptor *BLEClient::get_descriptor(uint16_t service, uint16_t chr, uint16_t descr) {
  return this->get_descriptor(espbt::ESPBTUUID::from_uint16(service), espbt::ESPBTUUID::from_uint16(chr),
                              espbt::ESPBTUUID::from_uint16(descr));
}

BLEService::~BLEService() {
  for (auto &chr : this->characteristics_)
    delete chr;
}

void BLEService::parse_characteristics() {
  uint16_t offset = 0;
  esp_gattc_char_elem_t result;

  while (true) {
    uint16_t count = 1;
    esp_gatt_status_t status =
        esp_ble_gattc_get_all_char(this->client_->gattc_if_, this->client_->conn_id_, this->start_handle_,
                                   this->end_handle_, &result, &count, offset);
    if (status == ESP_GATT_INVALID_OFFSET || status == ESP_GATT_NOT_FOUND) {
      break;
    }
    if (status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "esp_ble_gattc_get_all_char error, status=%d", status);
      break;
    }
    if (count == 0) {
      break;
    }

    BLECharacteristic *bChar = new BLECharacteristic();
    bChar->uuid_ = espbt::ESPBTUUID::from_uuid(result.uuid);
    bChar->properties_ = result.properties;
    bChar->handle_ = result.char_handle;
    bChar->service_ = this;
    this->characteristics_.push_back(bChar);
    ESP_LOGI(TAG, " characteristic %s, handle 0x%x, properties 0x%x", bChar->uuid_.to_string().c_str(), bChar->handle_,
             bChar->properties_);
    bChar->parse_descriptors();
    offset++;
  }
}

BLECharacteristic::~BLECharacteristic() {
  for (auto &desc : this->descriptors_)
    delete desc;
}

void BLECharacteristic::parse_descriptors() {
  uint16_t offset = 0;
  esp_gattc_descr_elem_t result;

  while (true) {
    uint16_t count = 1;
    esp_gatt_status_t status = esp_ble_gattc_get_all_descr(
        this->service_->client_->gattc_if_, this->service_->client_->conn_id_, this->handle_, &result, &count, offset);
    if (status == ESP_GATT_INVALID_OFFSET || status == ESP_GATT_NOT_FOUND) {
      break;
    }
    if (status != ESP_GATT_OK) {
      ESP_LOGW(TAG, "esp_ble_gattc_get_all_descr error, status=%d", status);
      break;
    }
    if (count == 0) {
      break;
    }

    BLEDescriptor *b_desc = new BLEDescriptor();
    b_desc->uuid_ = espbt::ESPBTUUID::from_uuid(result.uuid);
    b_desc->handle_ = result.handle;
    b_desc->characteristic_ = this;
    this->descriptors_.push_back(b_desc);
    ESP_LOGI(TAG, "   descriptor %s, handle 0x%x", b_desc->uuid_.to_string().c_str(), b_desc->handle_);
    offset++;
  }
}

BLEDescriptor *BLECharacteristic::get_descriptor(espbt::ESPBTUUID uuid) {
  for (auto &desc : this->descriptors_)
    if (desc->uuid_ == uuid)
      return desc;
  return nullptr;
}
BLEDescriptor *BLECharacteristic::get_descriptor(uint16_t uuid) {
  return this->get_descriptor(espbt::ESPBTUUID::from_uint16(uuid));
}

}  // namespace ble_client
}  // namespace esphome

#endif
