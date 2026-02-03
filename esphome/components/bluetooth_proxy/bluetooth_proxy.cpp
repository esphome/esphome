#include "bluetooth_proxy.h"

#include "esphome/core/log.h"
#include "esphome/core/macros.h"
#include "esphome/core/application.h"
#include <cstring>

#ifdef USE_ESP32

namespace esphome::bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy";

// BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE is defined during code generation
// It sets the batch size for BLE advertisements to maximize WiFi efficiency

// Verify BLE advertisement data array size matches the BLE specification (31 bytes adv + 31 bytes scan response)
static_assert(sizeof(((api::BluetoothLERawAdvertisement *) nullptr)->data) == 62,
              "BLE advertisement data array size mismatch");

BluetoothProxy::BluetoothProxy() { global_bluetooth_proxy = this; }

void BluetoothProxy::setup() {
  this->connections_free_response_.limit = BLUETOOTH_PROXY_MAX_CONNECTIONS;
  this->connections_free_response_.free = BLUETOOTH_PROXY_MAX_CONNECTIONS;

  // Capture the configured scan mode from YAML before any API changes
  this->configured_scan_active_ = this->parent_->get_scan_active();

  this->parent_->add_scanner_state_listener(this);
}

void BluetoothProxy::on_scanner_state(esp32_ble_tracker::ScannerState state) {
  if (this->api_connection_ != nullptr) {
    this->send_bluetooth_scanner_state_(state);
  }
}

void BluetoothProxy::send_bluetooth_scanner_state_(esp32_ble_tracker::ScannerState state) {
  api::BluetoothScannerStateResponse resp;
  resp.state = static_cast<api::enums::BluetoothScannerState>(state);
  resp.mode = this->parent_->get_scan_active() ? api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE
                                               : api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_PASSIVE;
  resp.configured_mode = this->configured_scan_active_
                             ? api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE
                             : api::enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_PASSIVE;
  this->api_connection_->send_message(resp, api::BluetoothScannerStateResponse::MESSAGE_TYPE);
}

void BluetoothProxy::log_connection_request_ignored_(BluetoothConnection *connection, espbt::ClientState state) {
  ESP_LOGW(TAG, "[%d] [%s] Connection request ignored, state: %s", connection->get_connection_index(),
           connection->address_str(), espbt::client_state_to_string(state));
}

void BluetoothProxy::log_connection_info_(BluetoothConnection *connection, const char *message) {
  ESP_LOGI(TAG, "[%d] [%s] Connecting %s", connection->get_connection_index(), connection->address_str(), message);
}

void BluetoothProxy::log_not_connected_gatt_(const char *action, const char *type) {
  ESP_LOGW(TAG, "Cannot %s GATT %s, not connected", action, type);
}

void BluetoothProxy::handle_gatt_not_connected_(uint64_t address, uint16_t handle, const char *action,
                                                const char *type) {
  this->log_not_connected_gatt_(action, type);
  this->send_gatt_error(address, handle, ESP_GATT_NOT_CONNECTED);
}

#ifdef USE_ESP32_BLE_DEVICE
bool BluetoothProxy::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // This method should never be called since bluetooth_proxy always uses raw advertisements
  // but we need to provide an implementation to satisfy the virtual method requirement
  return false;
}
#endif

bool BluetoothProxy::parse_devices(const esp32_ble::BLEScanResult *scan_results, size_t count) {
  if (!api::global_api_server->is_connected() || this->api_connection_ == nullptr)
    return false;

  auto &advertisements = this->response_.advertisements;

  for (size_t i = 0; i < count; i++) {
    auto &result = scan_results[i];
    uint8_t length = result.adv_data_len + result.scan_rsp_len;

    // Fill in the data directly at current position
    auto &adv = advertisements[this->response_.advertisements_len];
    adv.address = esp32_ble::ble_addr_to_uint64(result.bda);
    adv.rssi = result.rssi;
    adv.address_type = result.ble_addr_type;
    adv.data_len = length;
    std::memcpy(adv.data, result.ble_adv, length);

    this->response_.advertisements_len++;

    ESP_LOGV(TAG, "Queuing raw packet from %02X:%02X:%02X:%02X:%02X:%02X, length %d. RSSI: %d dB", result.bda[0],
             result.bda[1], result.bda[2], result.bda[3], result.bda[4], result.bda[5], length, result.rssi);

    // Flush if we have reached BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE
    if (this->response_.advertisements_len >= BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE) {
      this->flush_pending_advertisements();
    }
  }

  return true;
}

void BluetoothProxy::flush_pending_advertisements() {
  if (this->response_.advertisements_len == 0 || !api::global_api_server->is_connected() ||
      this->api_connection_ == nullptr)
    return;

  // Send the message
  this->api_connection_->send_message(this->response_, api::BluetoothLERawAdvertisementsResponse::MESSAGE_TYPE);

  ESP_LOGV(TAG, "Sent batch of %u BLE advertisements", this->response_.advertisements_len);

  // Reset the length for the next batch
  this->response_.advertisements_len = 0;
}

void BluetoothProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Bluetooth Proxy:\n"
                "  Active: %s\n"
                "  Connections: %d",
                YESNO(this->active_), this->connection_count_);
}

void BluetoothProxy::loop() {
  if (!api::global_api_server->is_connected() || this->api_connection_ == nullptr) {
    for (uint8_t i = 0; i < this->connection_count_; i++) {
      auto *connection = this->connections_[i];
      if (connection->get_address() != 0 && !connection->disconnect_pending()) {
        connection->disconnect();
      }
    }
    return;
  }

  // Flush any pending BLE advertisements that have been accumulated but not yet sent
  uint32_t now = App.get_loop_component_start_time();

  // Flush accumulated advertisements every 100ms
  if (now - this->last_advertisement_flush_time_ >= 100) {
    this->flush_pending_advertisements();
    this->last_advertisement_flush_time_ = now;
  }
}

esp32_ble_tracker::AdvertisementParserType BluetoothProxy::get_advertisement_parser_type() {
  return esp32_ble_tracker::AdvertisementParserType::RAW_ADVERTISEMENTS;
}

BluetoothConnection *BluetoothProxy::get_connection_(uint64_t address, bool reserve) {
  for (uint8_t i = 0; i < this->connection_count_; i++) {
    auto *connection = this->connections_[i];
    uint64_t conn_addr = connection->get_address();

    if (conn_addr == address)
      return connection;

    if (reserve && conn_addr == 0) {
      connection->send_service_ = INIT_SENDING_SERVICES;
      connection->set_address(address);
      // All connections must start at INIT
      // We only set the state if we allocate the connection
      // to avoid a race where multiple connection attempts
      // are made.
      connection->set_state(espbt::ClientState::INIT);
      return connection;
    }
  }
  return nullptr;
}

void BluetoothProxy::bluetooth_device_request(const api::BluetoothDeviceRequest &msg) {
  switch (msg.request_type) {
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE:
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE: {
      auto *connection = this->get_connection_(msg.address, true);
      if (connection == nullptr) {
        ESP_LOGW(TAG, "No free connections available");
        this->send_device_connection(msg.address, false);
        return;
      }
      if (!msg.has_address_type) {
        ESP_LOGE(TAG, "[%d] [%s] Missing address type in connect request", connection->get_connection_index(),
                 connection->address_str());
        this->send_device_connection(msg.address, false);
        return;
      }
      if (connection->state() == espbt::ClientState::CONNECTED ||
          connection->state() == espbt::ClientState::ESTABLISHED) {
        this->log_connection_request_ignored_(connection, connection->state());
        this->send_device_connection(msg.address, true);
        this->send_connections_free();
        return;
      } else if (connection->state() == espbt::ClientState::CONNECTING) {
        if (connection->disconnect_pending()) {
          ESP_LOGW(TAG, "[%d] [%s] Connection request while pending disconnect, cancelling pending disconnect",
                   connection->get_connection_index(), connection->address_str());
          connection->cancel_pending_disconnect();
          return;
        }
        this->log_connection_request_ignored_(connection, connection->state());
        return;
      } else if (connection->state() != espbt::ClientState::INIT) {
        this->log_connection_request_ignored_(connection, connection->state());
        return;
      }
      if (msg.request_type == api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE) {
        connection->set_connection_type(espbt::ConnectionType::V3_WITH_CACHE);
        this->log_connection_info_(connection, "v3 with cache");
      } else {  // BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE
        connection->set_connection_type(espbt::ConnectionType::V3_WITHOUT_CACHE);
        this->log_connection_info_(connection, "v3 without cache");
      }
      uint64_to_bd_addr(msg.address, connection->remote_bda_);
      connection->set_remote_addr_type(static_cast<esp_ble_addr_type_t>(msg.address_type));
      connection->set_state(espbt::ClientState::DISCOVERED);
      this->send_connections_free();
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT: {
      auto *connection = this->get_connection_(msg.address, false);
      if (connection == nullptr) {
        this->send_device_connection(msg.address, false);
        this->send_connections_free();
        return;
      }
      if (connection->state() != espbt::ClientState::IDLE) {
        connection->disconnect();
      } else {
        connection->set_address(0);
        this->send_device_connection(msg.address, false);
        this->send_connections_free();
      }
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR: {
      auto *connection = this->get_connection_(msg.address, false);
      if (connection != nullptr) {
        if (!connection->is_paired()) {
          auto err = connection->pair();
          if (err != ESP_OK) {
            this->send_device_pairing(msg.address, false, err);
          }
        } else {
          this->send_device_pairing(msg.address, true);
        }
      }
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR: {
      esp_bd_addr_t address;
      uint64_to_bd_addr(msg.address, address);
      esp_err_t ret = esp_ble_remove_bond_device(address);
      this->send_device_pairing(msg.address, ret == ESP_OK, ret);
      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CLEAR_CACHE: {
      esp_bd_addr_t address;
      uint64_to_bd_addr(msg.address, address);
      esp_err_t ret = esp_ble_gattc_cache_clean(address);
      api::BluetoothDeviceClearCacheResponse call;
      call.address = msg.address;
      call.success = ret == ESP_OK;
      call.error = ret;

      this->api_connection_->send_message(call, api::BluetoothDeviceClearCacheResponse::MESSAGE_TYPE);

      break;
    }
    case api::enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT: {
      ESP_LOGE(TAG, "V1 connections removed");
      this->send_device_connection(msg.address, false);
      break;
    }
  }
}

void BluetoothProxy::bluetooth_gatt_read(const api::BluetoothGATTReadRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "read", "characteristic");
    return;
  }

  auto err = connection->read_characteristic(msg.handle);
  if (err != ESP_OK) {
    this->send_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_write(const api::BluetoothGATTWriteRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "write", "characteristic");
    return;
  }

  auto err = connection->write_characteristic(msg.handle, msg.data, msg.data_len, msg.response);
  if (err != ESP_OK) {
    this->send_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_read_descriptor(const api::BluetoothGATTReadDescriptorRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "read", "descriptor");
    return;
  }

  auto err = connection->read_descriptor(msg.handle);
  if (err != ESP_OK) {
    this->send_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_write_descriptor(const api::BluetoothGATTWriteDescriptorRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "write", "descriptor");
    return;
  }

  auto err = connection->write_descriptor(msg.handle, msg.data, msg.data_len, true);
  if (err != ESP_OK) {
    this->send_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::bluetooth_gatt_send_services(const api::BluetoothGATTGetServicesRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr || !connection->connected()) {
    this->handle_gatt_not_connected_(msg.address, 0, "get", "services");
    return;
  }
  if (!connection->service_count_) {
    ESP_LOGW(TAG, "[%d] [%s] No GATT services found", connection->connection_index_, connection->address_str());
    this->send_gatt_services_done(msg.address);
    return;
  }
  if (connection->send_service_ == INIT_SENDING_SERVICES)  // Start sending services if not started yet
    connection->send_service_ = 0;
}

void BluetoothProxy::bluetooth_gatt_notify(const api::BluetoothGATTNotifyRequest &msg) {
  auto *connection = this->get_connection_(msg.address, false);
  if (connection == nullptr) {
    this->handle_gatt_not_connected_(msg.address, msg.handle, "notify", "characteristic");
    return;
  }

  auto err = connection->notify_characteristic(msg.handle, msg.enable);
  if (err != ESP_OK) {
    this->send_gatt_error(msg.address, msg.handle, err);
  }
}

void BluetoothProxy::subscribe_api_connection(api::APIConnection *api_connection, uint32_t flags) {
  if (this->api_connection_ != nullptr) {
    ESP_LOGE(TAG, "Only one API subscription is allowed at a time");
    return;
  }
  this->api_connection_ = api_connection;
  this->parent_->recalculate_advertisement_parser_types();

  this->send_bluetooth_scanner_state_(this->parent_->get_scanner_state());
}

void BluetoothProxy::unsubscribe_api_connection(api::APIConnection *api_connection) {
  if (this->api_connection_ != api_connection) {
    ESP_LOGV(TAG, "API connection is not subscribed");
    return;
  }
  this->api_connection_ = nullptr;
  this->parent_->recalculate_advertisement_parser_types();
}

void BluetoothProxy::send_device_connection(uint64_t address, bool connected, uint16_t mtu, esp_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothDeviceConnectionResponse call;
  call.address = address;
  call.connected = connected;
  call.mtu = mtu;
  call.error = error;
  this->api_connection_->send_message(call, api::BluetoothDeviceConnectionResponse::MESSAGE_TYPE);
}
void BluetoothProxy::send_connections_free() {
  if (this->api_connection_ != nullptr) {
    this->send_connections_free(this->api_connection_);
  }
}

void BluetoothProxy::send_connections_free(api::APIConnection *api_connection) {
  api_connection->send_message(this->connections_free_response_, api::BluetoothConnectionsFreeResponse::MESSAGE_TYPE);
}

void BluetoothProxy::send_gatt_services_done(uint64_t address) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothGATTGetServicesDoneResponse call;
  call.address = address;
  this->api_connection_->send_message(call, api::BluetoothGATTGetServicesDoneResponse::MESSAGE_TYPE);
}

void BluetoothProxy::send_gatt_error(uint64_t address, uint16_t handle, esp_err_t error) {
  if (this->api_connection_ == nullptr)
    return;
  api::BluetoothGATTErrorResponse call;
  call.address = address;
  call.handle = handle;
  call.error = error;
  this->api_connection_->send_message(call, api::BluetoothGATTWriteResponse::MESSAGE_TYPE);
}

void BluetoothProxy::send_device_pairing(uint64_t address, bool paired, esp_err_t error) {
  api::BluetoothDevicePairingResponse call;
  call.address = address;
  call.paired = paired;
  call.error = error;

  this->api_connection_->send_message(call, api::BluetoothDevicePairingResponse::MESSAGE_TYPE);
}

void BluetoothProxy::send_device_unpairing(uint64_t address, bool success, esp_err_t error) {
  api::BluetoothDeviceUnpairingResponse call;
  call.address = address;
  call.success = success;
  call.error = error;

  this->api_connection_->send_message(call, api::BluetoothDeviceUnpairingResponse::MESSAGE_TYPE);
}

void BluetoothProxy::bluetooth_scanner_set_mode(bool active) {
  if (this->parent_->get_scan_active() == active) {
    return;
  }
  ESP_LOGD(TAG, "Setting scanner mode to %s", active ? "active" : "passive");
  this->parent_->set_scan_active(active);
  this->parent_->stop_scan();
  this->parent_->set_scan_continuous(
      true);  // Set this to true to automatically start scanning again when it has cleaned up.
}

BluetoothProxy *global_bluetooth_proxy = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::bluetooth_proxy

#endif  // USE_ESP32
