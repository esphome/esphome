#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "ble_client.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_client {

static const char *const TAG = "ble_client";

float BLEClient::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void BLEClient::setup() {
  auto ret = esp_ble_gattc_app_register(this->app_id);
  if (ret) {
    ESP_LOGE(TAG, "gattc app register failed. app_id=%d code=%d", this->app_id, ret);
    this->mark_failed();
  }
  this->set_states_(espbt::ClientState::IDLE);
  this->enabled = true;
}

void BLEClient::loop() {
  if (this->state() == espbt::ClientState::DISCOVERED) {
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
  if (!this->enabled)
    return false;
  if (device.address_uint64() != this->address)
    return false;
  if (this->state() != espbt::ClientState::IDLE)
    return false;

  ESP_LOGD(TAG, "Found device at MAC address [%s]", device.address_str().c_str());
  this->set_states_(espbt::ClientState::DISCOVERED);

  auto addr = device.address_uint64();
  this->remote_bda[0] = (addr >> 40) & 0xFF;
  this->remote_bda[1] = (addr >> 32) & 0xFF;
  this->remote_bda[2] = (addr >> 24) & 0xFF;
  this->remote_bda[3] = (addr >> 16) & 0xFF;
  this->remote_bda[4] = (addr >> 8) & 0xFF;
  this->remote_bda[5] = (addr >> 0) & 0xFF;
  return true;
}

std::string BLEClient::address_str() const {
  char buf[20];
  sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", (uint8_t)(this->address >> 40) & 0xff,
          (uint8_t)(this->address >> 32) & 0xff, (uint8_t)(this->address >> 24) & 0xff,
          (uint8_t)(this->address >> 16) & 0xff, (uint8_t)(this->address >> 8) & 0xff,
          (uint8_t)(this->address >> 0) & 0xff);
  std::string ret;
  ret = buf;
  return ret;
}

void BLEClient::set_enabled(bool enabled) {
  if (enabled == this->enabled)
    return;
  if (!enabled && this->state() != espbt::ClientState::IDLE) {
    ESP_LOGI(TAG, "[%s] Disabling BLE client.", this->address_str().c_str());
    auto ret = esp_ble_gattc_close(this->gattc_if, this->conn_id);
    if (ret) {
      ESP_LOGW(TAG, "esp_ble_gattc_close error, address=%s status=%d", this->address_str().c_str(), ret);
    }
  }
  this->enabled = enabled;
}

void BLEClient::connect() {
  ESP_LOGI(TAG, "Attempting BLE connection to %s", this->address_str().c_str());
  auto ret = esp_ble_gattc_open(this->gattc_if, this->remote_bda, BLE_ADDR_TYPE_PUBLIC, true);
  if (ret) {
    ESP_LOGW(TAG, "esp_ble_gattc_open error, address=%s status=%d", this->address_str().c_str(), ret);
    this->set_states_(espbt::ClientState::IDLE);
  } else {
    this->set_states_(espbt::ClientState::CONNECTING);
  }
}

void BLEClient::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t esp_gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
  if (event == ESP_GATTC_REG_EVT && this->app_id != param->reg.app_id)
    return;
  if (event != ESP_GATTC_REG_EVT && esp_gattc_if != ESP_GATT_IF_NONE && esp_gattc_if != this->gattc_if)
    return;

  bool all_established = this->all_nodes_established_();

  switch (event) {
    case ESP_GATTC_REG_EVT: {
      if (param->reg.status == ESP_GATT_OK) {
        ESP_LOGV(TAG, "gattc registered app id %d", this->app_id);
        this->gattc_if = esp_gattc_if;
      } else {
        ESP_LOGE(TAG, "gattc app registration failed id=%d code=%d", param->reg.app_id, param->reg.status);
      }
      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      ESP_LOGV(TAG, "[%s] ESP_GATTC_OPEN_EVT", this->address_str().c_str());
      if (param->open.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "connect to %s failed, status=%d", this->address_str().c_str(), param->open.status);
        this->set_states_(espbt::ClientState::IDLE);
        break;
      }
      this->conn_id = param->open.conn_id;
      auto ret = esp_ble_gattc_send_mtu_req(this->gattc_if, param->open.conn_id);
      if (ret) {
        ESP_LOGW(TAG, "esp_ble_gattc_send_mtu_req failed, status=%d", ret);
      }
      break;
    }
    case ESP_GATTC_CFG_MTU_EVT: {
      if (param->cfg_mtu.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "cfg_mtu to %s failed, status %d", this->address_str().c_str(), param->cfg_mtu.status);
        this->set_states_(espbt::ClientState::IDLE);
        break;
      }
      ESP_LOGV(TAG, "cfg_mtu status %d, mtu %d", param->cfg_mtu.status, param->cfg_mtu.mtu);
      esp_ble_gattc_search_service(esp_gattc_if, param->cfg_mtu.conn_id, nullptr);
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      if (memcmp(param->disconnect.remote_bda, this->remote_bda, 6) != 0) {
        return;
      }
      ESP_LOGV(TAG, "[%s] ESP_GATTC_DISCONNECT_EVT", this->address_str().c_str());
      for (auto &svc : this->services_)
        delete svc;  // NOLINT(cppcoreguidelines-owning-memory)
      this->services_.clear();
      this->set_states_(espbt::ClientState::IDLE);
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
      this->set_states_(espbt::ClientState::CONNECTED);
      this->set_state(espbt::ClientState::ESTABLISHED);
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      auto descr = this->get_config_descriptor(param->reg_for_notify.handle);
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
      uint8_t notify_en = 1;
      auto status = esp_ble_gattc_write_char_descr(this->gattc_if, this->conn_id, descr->handle, sizeof(notify_en),
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
    node->gattc_event_handler(event, esp_gattc_if, param);

  // Delete characteristics after clients have used them to save RAM.
  if (!all_established && this->all_nodes_established_()) {
    for (auto &svc : this->services_)
      delete svc;  // NOLINT(cppcoreguidelines-owning-memory)
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
      if (length > 2) {
        return (float) ((uint16_t)(value[1] << 8) + (uint16_t) value[2]);
      }
    case 0x7:  // uint24.
      if (length > 3) {
        return (float) ((uint32_t)(value[1] << 16) + (uint32_t)(value[2] << 8) + (uint32_t)(value[3]));
      }
    case 0x8:  // uint32.
      if (length > 4) {
        return (float) ((uint32_t)(value[1] << 24) + (uint32_t)(value[2] << 16) + (uint32_t)(value[3] << 8) +
                        (uint32_t)(value[4]));
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

BLEService *BLEClient::get_service(espbt::ESPBTUUID uuid) {
  for (auto svc : this->services_)
    if (svc->uuid == uuid)
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
    for (auto &chr : svc->characteristics)
      if (chr->handle == handle)
        for (auto &desc : chr->descriptors)
          if (desc->uuid == espbt::ESPBTUUID::from_uint16(0x2902))
            return desc;
  return nullptr;
}

BLECharacteristic *BLEService::get_characteristic(espbt::ESPBTUUID uuid) {
  for (auto &chr : this->characteristics)
    if (chr->uuid == uuid)
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
  for (auto &chr : this->characteristics)
    delete chr;  // NOLINT(cppcoreguidelines-owning-memory)
}

void BLEService::parse_characteristics() {
  uint16_t offset = 0;
  esp_gattc_char_elem_t result;

  while (true) {
    uint16_t count = 1;
    esp_gatt_status_t status = esp_ble_gattc_get_all_char(
        this->client->gattc_if, this->client->conn_id, this->start_handle, this->end_handle, &result, &count, offset);
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

    BLECharacteristic *characteristic = new BLECharacteristic();  // NOLINT(cppcoreguidelines-owning-memory)
    characteristic->uuid = espbt::ESPBTUUID::from_uuid(result.uuid);
    characteristic->properties = result.properties;
    characteristic->handle = result.char_handle;
    characteristic->service = this;
    this->characteristics.push_back(characteristic);
    ESP_LOGI(TAG, " characteristic %s, handle 0x%x, properties 0x%x", characteristic->uuid.to_string().c_str(),
             characteristic->handle, characteristic->properties);
    characteristic->parse_descriptors();
    offset++;
  }
}

BLECharacteristic::~BLECharacteristic() {
  for (auto &desc : this->descriptors)
    delete desc;  // NOLINT(cppcoreguidelines-owning-memory)
}

void BLECharacteristic::parse_descriptors() {
  uint16_t offset = 0;
  esp_gattc_descr_elem_t result;

  while (true) {
    uint16_t count = 1;
    esp_gatt_status_t status = esp_ble_gattc_get_all_descr(
        this->service->client->gattc_if, this->service->client->conn_id, this->handle, &result, &count, offset);
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

    BLEDescriptor *desc = new BLEDescriptor();  // NOLINT(cppcoreguidelines-owning-memory)
    desc->uuid = espbt::ESPBTUUID::from_uuid(result.uuid);
    desc->handle = result.handle;
    desc->characteristic = this;
    this->descriptors.push_back(desc);
    ESP_LOGV(TAG, "   descriptor %s, handle 0x%x", desc->uuid.to_string().c_str(), desc->handle);
    offset++;
  }
}

BLEDescriptor *BLECharacteristic::get_descriptor(espbt::ESPBTUUID uuid) {
  for (auto &desc : this->descriptors)
    if (desc->uuid == uuid)
      return desc;
  return nullptr;
}
BLEDescriptor *BLECharacteristic::get_descriptor(uint16_t uuid) {
  return this->get_descriptor(espbt::ESPBTUUID::from_uint16(uuid));
}

void BLECharacteristic::write_value(uint8_t *new_val, int16_t new_val_size) {
  auto client = this->service->client;
  auto status = esp_ble_gattc_write_char(client->gattc_if, client->conn_id, this->handle, new_val_size, new_val,
                                         ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status) {
    ESP_LOGW(TAG, "Error sending write value to BLE gattc server, status=%d", status);
  }
}

}  // namespace ble_client
}  // namespace esphome

#endif
