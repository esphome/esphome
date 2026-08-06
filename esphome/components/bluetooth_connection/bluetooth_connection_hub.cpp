#include "bluetooth_connection_hub.h"

#if !defined(USE_ESP32) && defined(USE_BLE_GATT_CLIENT)

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/bluetooth_proxy/bluetooth_proxy.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::bluetooth_connection {

static const char *const TAG = "bluetooth_connection";

void BluetoothConnection::set_address(uint64_t address) {
  // Keep the proxy's pre-allocated connections-free message in step
  this->proxy_->update_address_slot_(this->address_, address);
  this->address_ = address;
  if (address == 0) {
    this->address_str_[0] = '\0';
    return;
  }
  uint8_t mac[6];
  ble_device_base::uint64_to_mac_msb_first(address, mac);
  format_mac_addr_upper(mac, this->address_str_);
}

void BluetoothConnection::start_connect() {
  this->state_ = ClientState::CONNECTING;
  int err = this->backend_->connect(this->address_, this->remote_addr_type_);
  if (err != 0) {
    ESP_LOGW(TAG, "[%d] [%s] connect failed, err=%d", this->connection_index_, this->address_str_, err);
    this->reset_connection_(err);
  }
}

void BluetoothConnection::disconnect() {
  // Idempotent like the esp32 class: the proxy's teardown loop calls this
  // every 100 ms while the API subscriber is gone, and a repeat call must not
  // reach the backend (whose busy error would free the slot mid-teardown).
  if (this->state_ == ClientState::IDLE || this->state_ == ClientState::DISCONNECTING) {
    return;
  }
  int err = this->backend_->disconnect();
  if (err != 0) {
    ESP_LOGW(TAG, "[%d] [%s] disconnect failed, err=%d", this->connection_index_, this->address_str_, err);
    // Backend refused (already idle): free the slot so the client is not stuck.
    this->reset_connection_(err);
    return;
  }
  this->state_ = ClientState::DISCONNECTING;
}

void BluetoothConnection::reset_connection_(conn_err_t reason) {
  this->state_ = ClientState::IDLE;
  this->services_discovered_ = false;
  this->backend_->release_services();
  this->proxy_->reset_connection_slot_(this, reason);
}

// ---- GattClientEventListener ----

void BluetoothConnection::on_connection_state(bool connected, uint16_t mtu, int error) {
  if (connected) {
    this->mtu_ = mtu;
    if (this->connection_type_ == ConnectionType::V3_WITH_CACHE) {
      // The API client has the services cached; never discover them. No
      // discovery phase needs the fast interval, so settle straight into the
      // shared steady-state parameters (same lifecycle place as esp32).
      this->state_ = ClientState::ESTABLISHED;
      this->backend_->update_connection_params(ble_device_base::MEDIUM_MIN_CONN_INTERVAL,
                                               ble_device_base::MEDIUM_MAX_CONN_INTERVAL, 0,
                                               ble_device_base::MEDIUM_CONN_TIMEOUT);
      this->proxy_->send_device_connection(this->address_, true, mtu);
      this->proxy_->send_connections_free();
      return;
    }
    // V3_WITHOUT_CACHE: discover services first — the connected response is
    // sent when discovery completes, mirroring the esp32 flow (MTU + services
    // before the response).
    this->state_ = ClientState::CONNECTED;
    int err = this->backend_->discover_services();
    if (err != 0) {
      ESP_LOGW(TAG, "[%d] [%s] discover_services failed, err=%d", this->connection_index_, this->address_str_, err);
      this->disconnect();
    }
    return;
  }
  // Disconnected, connect failed, or teardown complete
  if (this->address_ == 0) {
    return;  // Slot already freed
  }
  ESP_LOGD(TAG, "[%d] [%s] Disconnected, reason=0x%02x, freeing slot", this->connection_index_, this->address_str_,
           error);
  this->reset_connection_(error);
}

void BluetoothConnection::on_service_discovery_done(int error) {
  ESP_LOGD(TAG, "[%d] [%s] Discovery finished (err=%d), sending connected (mtu=%u)", this->connection_index_,
           this->address_str_, error, this->mtu_);
  if (error != 0) {
    ESP_LOGW(TAG, "[%d] [%s] Service discovery failed, err=%d", this->connection_index_, this->address_str_, error);
    this->disconnect();
    return;
  }
  this->state_ = ClientState::ESTABLISHED;
  this->services_discovered_ = true;
  this->proxy_->send_device_connection(this->address_, true, this->mtu_);
  this->proxy_->send_connections_free();
}

void BluetoothConnection::log_gatt_operation_error_(const char *operation, uint16_t handle, int status) {
  ESP_LOGW(TAG, "[%d] [%s] Error %s for handle 0x%2X, status=%d", this->connection_index_, this->address_str_,
           operation, handle, status);
}

void BluetoothConnection::on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) {
  if (error != 0) {
    this->log_gatt_operation_error_("reading char/descriptor", handle, error);
    this->proxy_->send_gatt_error(this->address_, handle, error);
    return;
  }
  auto *api_connection = this->proxy_->get_api_connection();
  if (api_connection == nullptr)
    return;
  api::BluetoothGATTReadResponse resp;
  resp.address = this->address_;
  resp.handle = handle;
  resp.set_data(data, len);
  api_connection->send_message(resp);
}

void BluetoothConnection::on_write_result(uint16_t handle, int error) {
  if (error != 0) {
    this->log_gatt_operation_error_("writing char/descriptor", handle, error);
    this->proxy_->send_gatt_error(this->address_, handle, error);
    return;
  }
  auto *api_connection = this->proxy_->get_api_connection();
  if (api_connection == nullptr)
    return;
  api::BluetoothGATTWriteResponse resp;
  resp.address = this->address_;
  resp.handle = handle;
  api_connection->send_message(resp);
}

void BluetoothConnection::on_notify_state(uint16_t handle, bool enabled, int error) {
  if (error != 0) {
    this->log_gatt_operation_error_(enabled ? "registering notifications" : "unregistering notifications", handle,
                                    error);
    this->proxy_->send_gatt_error(this->address_, handle, error);
    return;
  }
  auto *api_connection = this->proxy_->get_api_connection();
  if (api_connection == nullptr)
    return;
  api::BluetoothGATTNotifyResponse resp;
  resp.address = this->address_;
  resp.handle = handle;
  api_connection->send_message(resp);
}

void BluetoothConnection::on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) {
  ESP_LOGV(TAG, "[%d] [%s] Notify: handle=0x%2X", this->connection_index_, this->address_str_, handle);
  auto *api_connection = this->proxy_->get_api_connection();
  if (api_connection == nullptr)
    return;
  api::BluetoothGATTNotifyDataResponse resp;
  resp.address = this->address_;
  resp.handle = handle;
  resp.set_data(data, len);
  api_connection->send_message(resp);
}

// ---- GATT operations ----

conn_err_t BluetoothConnection::check_connected_op_(const char *action, const char *type) const {
  if (this->connected()) {
    return CONN_OK;
  }
  ESP_LOGW(TAG, "[%d] [%s] Cannot %s GATT %s, not connected.", this->connection_index_, this->address_str_, action,
           type);
  return GATT_NOT_CONNECTED;
}

conn_err_t BluetoothConnection::read_characteristic(uint16_t handle) {
  if (conn_err_t err = this->check_connected_op_("read", "characteristic"); err != CONN_OK)
    return err;
  ESP_LOGV(TAG, "[%d] [%s] Reading GATT characteristic handle %d", this->connection_index_, this->address_str_, handle);
  return this->backend_->read_characteristic(handle);
}

conn_err_t BluetoothConnection::write_characteristic(uint16_t handle, const uint8_t *data, size_t length,
                                                     bool response) {
  if (conn_err_t err = this->check_connected_op_("write", "characteristic"); err != CONN_OK)
    return err;
  ESP_LOGV(TAG, "[%d] [%s] Writing GATT characteristic handle %d", this->connection_index_, this->address_str_, handle);
  return this->backend_->write_characteristic(handle, data, static_cast<uint16_t>(length), response);
}

conn_err_t BluetoothConnection::read_descriptor(uint16_t handle) {
  if (conn_err_t err = this->check_connected_op_("read", "descriptor"); err != CONN_OK)
    return err;
  ESP_LOGV(TAG, "[%d] [%s] Reading GATT descriptor handle %d", this->connection_index_, this->address_str_, handle);
  return this->backend_->read_descriptor(handle);
}

conn_err_t BluetoothConnection::write_descriptor(uint16_t handle, const uint8_t *data, size_t length,
                                                 bool /*response*/) {
  if (conn_err_t err = this->check_connected_op_("write", "descriptor"); err != CONN_OK)
    return err;
  ESP_LOGV(TAG, "[%d] [%s] Writing GATT descriptor handle %d", this->connection_index_, this->address_str_, handle);
  return this->backend_->write_descriptor(handle, data, static_cast<uint16_t>(length));
}

conn_err_t BluetoothConnection::notify_characteristic(uint16_t handle, bool enable) {
  if (conn_err_t err = this->check_connected_op_("notify", "characteristic"); err != CONN_OK)
    return err;
  ESP_LOGV(TAG, "[%d] [%s] %s GATT characteristic notifications handle %d", this->connection_index_, this->address_str_,
           enable ? "Registering for" : "Unregistering for", handle);
  return this->backend_->notify_characteristic(handle, enable);
}

conn_err_t BluetoothConnection::update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                                         uint16_t timeout) {
  if (conn_err_t err = this->check_connected_op_("update params of", "connection"); err != CONN_OK)
    return err;
  return this->backend_->update_connection_params(min_interval, max_interval, latency, timeout);
}

// ---- Service streaming ----

void BluetoothConnection::send_service_for_discovery_() {
  auto table = this->backend_->get_service_table();
  if (this->send_service_ >= static_cast<int16_t>(table.service_count)) {
    this->send_service_ = DONE_SENDING_SERVICES;
    this->proxy_->send_gatt_services_done(this->address_);
    this->backend_->release_services();
    return;
  }

  // Early return if no API connection
  auto *api_conn = this->proxy_->get_api_connection();
  if (api_conn == nullptr) {
    this->send_service_ = DONE_SENDING_SERVICES;
    return;
  }

  // Check if client supports efficient UUIDs
  bool use_efficient_uuids = this->proxy_->client_supports_efficient_uuids();

  // Prepare response
  api::BluetoothGATTGetServicesResponse resp;
  resp.address = this->address_;

  // Dynamic batching based on actual size, same contract as the esp32 streamer
  size_t current_size = resp.calculate_size();

  while (this->send_service_ < static_cast<int16_t>(table.service_count)) {
    const auto &service = table.services[this->send_service_];

    // If this service likely won't fit, send current batch (unless it's the first)
    size_t estimated_size = estimate_service_size(service.characteristic_count, use_efficient_uuids);
    if (!resp.services.empty() && (current_size + estimated_size > MAX_PACKET_SIZE)) {
      break;
    }

    resp.services.emplace_back();
    auto &service_resp = resp.services.back();
    fill_gatt_uuid(service_resp.uuid, service_resp.short_uuid, service.uuid, use_efficient_uuids);
    service_resp.handle = service.start_handle;

    if (service.characteristic_count > 0) {
      service_resp.characteristics.init(service.characteristic_count);
      for (uint16_t ci = 0; ci < service.characteristic_count; ci++) {
        const auto &chr = table.characteristics[service.first_characteristic + ci];
        service_resp.characteristics.emplace_back();
        auto &characteristic_resp = service_resp.characteristics.back();
        fill_gatt_uuid(characteristic_resp.uuid, characteristic_resp.short_uuid, chr.uuid, use_efficient_uuids);
        characteristic_resp.handle = chr.value_handle;
        characteristic_resp.properties = chr.properties;
        if (chr.descriptor_count == 0) {
          continue;
        }
        characteristic_resp.descriptors.init(chr.descriptor_count);
        for (uint16_t di = 0; di < chr.descriptor_count; di++) {
          const auto &desc = table.descriptors[chr.first_descriptor + di];
          characteristic_resp.descriptors.emplace_back();
          auto &descriptor_resp = characteristic_resp.descriptors.back();
          fill_gatt_uuid(descriptor_resp.uuid, descriptor_resp.short_uuid, desc.uuid, use_efficient_uuids);
          descriptor_resp.handle = desc.handle;
        }
      }
    }

    if (close_service_batch(resp, current_size, this->send_service_, this->connection_index_, this->address_str_)) {
      break;
    }
  }

  // Send the message with dynamically batched services
  api_conn->send_message(resp);
}

}  // namespace esphome::bluetooth_connection

#endif  // !USE_ESP32 && USE_BLE_GATT_CLIENT
