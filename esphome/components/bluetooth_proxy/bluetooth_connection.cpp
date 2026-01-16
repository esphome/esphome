#include "bluetooth_connection.h"

#include "esphome/components/api/api_pb2.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "bluetooth_proxy.h"

namespace esphome::bluetooth_proxy {

static const char *const TAG = "bluetooth_proxy.connection";

// This function is allocation-free and directly packs UUIDs into the output array
// using precalculated constants for the Bluetooth base UUID
static void fill_128bit_uuid_array(std::array<uint64_t, 2> &out, esp_bt_uuid_t uuid_source) {
  // Bluetooth base UUID: 00000000-0000-1000-8000-00805F9B34FB
  // out[0] = bytes 8-15 (big-endian)
  // - For 128-bit UUIDs: use bytes 8-15 as-is
  // - For 16/32-bit UUIDs: insert into bytes 12-15, use 0x00001000 for bytes 8-11
  out[0] = uuid_source.len == ESP_UUID_LEN_128
               ? (((uint64_t) uuid_source.uuid.uuid128[15] << 56) | ((uint64_t) uuid_source.uuid.uuid128[14] << 48) |
                  ((uint64_t) uuid_source.uuid.uuid128[13] << 40) | ((uint64_t) uuid_source.uuid.uuid128[12] << 32) |
                  ((uint64_t) uuid_source.uuid.uuid128[11] << 24) | ((uint64_t) uuid_source.uuid.uuid128[10] << 16) |
                  ((uint64_t) uuid_source.uuid.uuid128[9] << 8) | ((uint64_t) uuid_source.uuid.uuid128[8]))
               : (((uint64_t) (uuid_source.len == ESP_UUID_LEN_16 ? uuid_source.uuid.uuid16 : uuid_source.uuid.uuid32)
                   << 32) |
                  0x00001000ULL);  // Base UUID bytes 8-11
  // out[1] = bytes 0-7 (big-endian)
  // - For 128-bit UUIDs: use bytes 0-7 as-is
  // - For 16/32-bit UUIDs: use precalculated base UUID constant
  out[1] = uuid_source.len == ESP_UUID_LEN_128
               ? ((uint64_t) uuid_source.uuid.uuid128[7] << 56) | ((uint64_t) uuid_source.uuid.uuid128[6] << 48) |
                     ((uint64_t) uuid_source.uuid.uuid128[5] << 40) | ((uint64_t) uuid_source.uuid.uuid128[4] << 32) |
                     ((uint64_t) uuid_source.uuid.uuid128[3] << 24) | ((uint64_t) uuid_source.uuid.uuid128[2] << 16) |
                     ((uint64_t) uuid_source.uuid.uuid128[1] << 8) | ((uint64_t) uuid_source.uuid.uuid128[0])
               : 0x800000805F9B34FBULL;  // Base UUID bytes 0-7: 80-00-00-80-5F-9B-34-FB
}

// Helper to fill UUID in the appropriate format based on client support and UUID type
static void fill_gatt_uuid(std::array<uint64_t, 2> &uuid_128, uint32_t &short_uuid, const esp_bt_uuid_t &uuid,
                           bool use_efficient_uuids) {
  if (!use_efficient_uuids || uuid.len == ESP_UUID_LEN_128) {
    // Use 128-bit format for old clients or when UUID is already 128-bit
    fill_128bit_uuid_array(uuid_128, uuid);
  } else if (uuid.len == ESP_UUID_LEN_16) {
    short_uuid = uuid.uuid.uuid16;
  } else if (uuid.len == ESP_UUID_LEN_32) {
    short_uuid = uuid.uuid.uuid32;
  }
}

// Constants for size estimation
static constexpr uint8_t SERVICE_OVERHEAD_LEGACY = 25;     // UUID(20) + handle(4) + overhead(1)
static constexpr uint8_t SERVICE_OVERHEAD_EFFICIENT = 10;  // UUID(6) + handle(4)
static constexpr uint8_t CHAR_SIZE_128BIT = 35;            // UUID(20) + handle(4) + props(4) + overhead(7)
static constexpr uint8_t DESC_SIZE_128BIT = 25;            // UUID(20) + handle(4) + overhead(1)
static constexpr uint8_t DESC_SIZE_16BIT = 10;             // UUID(6) + handle(4)
static constexpr uint8_t DESC_PER_CHAR = 1;                // Assume 1 descriptor per characteristic

// Helper to estimate service size before fetching all data
/**
 * Estimate the size of a Bluetooth service based on the number of characteristics and UUID format.
 *
 * @param char_count The number of characteristics in the service.
 * @param use_efficient_uuids Whether to use efficient UUIDs (16-bit or 32-bit) for newer APIVersions.
 * @return The estimated size of the service in bytes.
 *
 * This function calculates the size of a Bluetooth service by considering:
 * - A service overhead, which depends on whether efficient UUIDs are used.
 * - The size of each characteristic, assuming 128-bit UUIDs for safety.
 * - The size of descriptors, assuming one 128-bit descriptor per characteristic.
 */
static size_t estimate_service_size(uint16_t char_count, bool use_efficient_uuids) {
  size_t service_overhead = use_efficient_uuids ? SERVICE_OVERHEAD_EFFICIENT : SERVICE_OVERHEAD_LEGACY;
  // Always assume 128-bit UUIDs for characteristics to be safe
  size_t char_size = CHAR_SIZE_128BIT;
  // Assume one 128-bit descriptor per characteristic
  size_t desc_size = DESC_SIZE_128BIT * DESC_PER_CHAR;

  return service_overhead + (char_size + desc_size) * char_count;
}

bool BluetoothConnection::supports_efficient_uuids_() const {
  auto *api_conn = this->proxy_->get_api_connection();
  return api_conn && api_conn->client_supports_api_version(1, 12);
}

void BluetoothConnection::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Connection:");
  BLEClientBase::dump_config();
}

void BluetoothConnection::update_allocated_slot_(uint64_t find_value, uint64_t set_value) {
  auto &allocated = this->proxy_->connections_free_response_.allocated;
  for (auto &slot : allocated) {
    if (slot == find_value) {
      slot = set_value;
      return;
    }
  }
}

void BluetoothConnection::set_address(uint64_t address) {
  // If we're clearing an address (disconnecting), update the pre-allocated message
  if (address == 0 && this->address_ != 0) {
    this->proxy_->connections_free_response_.free++;
    this->update_allocated_slot_(this->address_, 0);
  }
  // If we're setting a new address (connecting), update the pre-allocated message
  else if (address != 0 && this->address_ == 0) {
    this->proxy_->connections_free_response_.free--;
    this->update_allocated_slot_(0, address);
  }

  // Call parent implementation to actually set the address
  BLEClientBase::set_address(address);
}

void BluetoothConnection::loop() {
  BLEClientBase::loop();

  // Early return if no active connection
  if (this->address_ == 0) {
    return;
  }

  // Handle service discovery if in valid range
  if (this->send_service_ >= 0 && this->send_service_ <= this->service_count_) {
    this->send_service_for_discovery_();
  }

  // Check if we should disable the loop
  // - For V3_WITH_CACHE: Services are never sent, disable after INIT state
  // - For V3_WITHOUT_CACHE: Disable only after service discovery is complete
  //   (send_service_ == DONE_SENDING_SERVICES, which is only set after services are sent)
  if (this->state_ != espbt::ClientState::INIT && (this->connection_type_ == espbt::ConnectionType::V3_WITH_CACHE ||
                                                   this->send_service_ == DONE_SENDING_SERVICES)) {
    this->disable_loop();
  }
}

void BluetoothConnection::reset_connection_(esp_err_t reason) {
  // Send disconnection notification
  this->proxy_->send_device_connection(this->address_, false, 0, reason);

  // Important: If we were in the middle of sending services, we do NOT send
  // send_gatt_services_done() here. This ensures the client knows that
  // the service discovery was interrupted and can retry. The client
  // (aioesphomeapi) implements a 30-second timeout (DEFAULT_BLE_TIMEOUT)
  // to detect incomplete service discovery rather than relying on us to
  // tell them about a partial list.
  this->set_address(0);
  this->send_service_ = INIT_SENDING_SERVICES;
  this->proxy_->send_connections_free();
}

void BluetoothConnection::send_service_for_discovery_() {
  if (this->send_service_ >= this->service_count_) {
    this->send_service_ = DONE_SENDING_SERVICES;
    this->proxy_->send_gatt_services_done(this->address_);
    this->release_services();
    return;
  }

  // Early return if no API connection
  auto *api_conn = this->proxy_->get_api_connection();
  if (api_conn == nullptr) {
    this->send_service_ = DONE_SENDING_SERVICES;
    return;
  }

  // Check if client supports efficient UUIDs
  bool use_efficient_uuids = this->supports_efficient_uuids_();

  // Prepare response
  api::BluetoothGATTGetServicesResponse resp;
  resp.address = this->address_;

  // Dynamic batching based on actual size
  // Conservative MTU limit for API messages (accounts for WPA3 overhead)
  static constexpr size_t MAX_PACKET_SIZE = 1360;

  // Keep running total of actual message size
  size_t current_size = 0;
  api::ProtoSize size;
  resp.calculate_size(size);
  current_size = size.get_size();

  while (this->send_service_ < this->service_count_) {
    esp_gattc_service_elem_t service_result;
    uint16_t service_count = 1;
    esp_gatt_status_t service_status = esp_ble_gattc_get_service(this->gattc_if_, this->conn_id_, nullptr,
                                                                 &service_result, &service_count, this->send_service_);

    if (service_status != ESP_GATT_OK || service_count == 0) {
      ESP_LOGE(TAG, "[%d] [%s] esp_ble_gattc_get_service %s, status=%d, service_count=%d, offset=%d",
               this->connection_index_, this->address_str(), service_status != ESP_GATT_OK ? "error" : "missing",
               service_status, service_count, this->send_service_);
      this->send_service_ = DONE_SENDING_SERVICES;
      return;
    }

    // Get the number of characteristics BEFORE adding to response
    uint16_t total_char_count = 0;
    esp_gatt_status_t char_count_status =
        esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_CHARACTERISTIC,
                                     service_result.start_handle, service_result.end_handle, 0, &total_char_count);

    if (char_count_status != ESP_GATT_OK) {
      this->log_connection_error_("esp_ble_gattc_get_attr_count", char_count_status);
      this->send_service_ = DONE_SENDING_SERVICES;
      return;
    }

    // If this service likely won't fit, send current batch (unless it's the first)
    size_t estimated_size = estimate_service_size(total_char_count, use_efficient_uuids);
    if (!resp.services.empty() && (current_size + estimated_size > MAX_PACKET_SIZE)) {
      // This service likely won't fit, send current batch
      break;
    }

    // Now add the service since we know it will likely fit
    resp.services.emplace_back();
    auto &service_resp = resp.services.back();

    fill_gatt_uuid(service_resp.uuid, service_resp.short_uuid, service_result.uuid, use_efficient_uuids);

    service_resp.handle = service_result.start_handle;

    if (total_char_count > 0) {
      // Initialize FixedVector with exact count and process characteristics
      service_resp.characteristics.init(total_char_count);
      uint16_t char_offset = 0;
      esp_gattc_char_elem_t char_result;
      while (true) {  // characteristics
        uint16_t char_count = 1;
        esp_gatt_status_t char_status =
            esp_ble_gattc_get_all_char(this->gattc_if_, this->conn_id_, service_result.start_handle,
                                       service_result.end_handle, &char_result, &char_count, char_offset);
        if (char_status == ESP_GATT_INVALID_OFFSET || char_status == ESP_GATT_NOT_FOUND) {
          break;
        }
        if (char_status != ESP_GATT_OK) {
          this->log_connection_error_("esp_ble_gattc_get_all_char", char_status);
          this->send_service_ = DONE_SENDING_SERVICES;
          return;
        }
        if (char_count == 0) {
          break;
        }

        service_resp.characteristics.emplace_back();
        auto &characteristic_resp = service_resp.characteristics.back();
        fill_gatt_uuid(characteristic_resp.uuid, characteristic_resp.short_uuid, char_result.uuid, use_efficient_uuids);
        characteristic_resp.handle = char_result.char_handle;
        characteristic_resp.properties = char_result.properties;
        char_offset++;

        // Get the number of descriptors directly with one call
        uint16_t total_desc_count = 0;
        esp_gatt_status_t desc_count_status = esp_ble_gattc_get_attr_count(
            this->gattc_if_, this->conn_id_, ESP_GATT_DB_DESCRIPTOR, 0, 0, char_result.char_handle, &total_desc_count);

        if (desc_count_status != ESP_GATT_OK) {
          this->log_connection_error_("esp_ble_gattc_get_attr_count", desc_count_status);
          this->send_service_ = DONE_SENDING_SERVICES;
          return;
        }
        if (total_desc_count == 0) {
          continue;
        }

        // Initialize FixedVector with exact count and process descriptors
        characteristic_resp.descriptors.init(total_desc_count);
        uint16_t desc_offset = 0;
        esp_gattc_descr_elem_t desc_result;
        while (true) {  // descriptors
          uint16_t desc_count = 1;
          esp_gatt_status_t desc_status = esp_ble_gattc_get_all_descr(
              this->gattc_if_, this->conn_id_, char_result.char_handle, &desc_result, &desc_count, desc_offset);
          if (desc_status == ESP_GATT_INVALID_OFFSET || desc_status == ESP_GATT_NOT_FOUND) {
            break;
          }
          if (desc_status != ESP_GATT_OK) {
            this->log_connection_error_("esp_ble_gattc_get_all_descr", desc_status);
            this->send_service_ = DONE_SENDING_SERVICES;
            return;
          }
          if (desc_count == 0) {
            break;  // No more descriptors
          }

          characteristic_resp.descriptors.emplace_back();
          auto &descriptor_resp = characteristic_resp.descriptors.back();
          fill_gatt_uuid(descriptor_resp.uuid, descriptor_resp.short_uuid, desc_result.uuid, use_efficient_uuids);
          descriptor_resp.handle = desc_result.handle;
          desc_offset++;
        }
      }
    }  // end if (total_char_count > 0)

    // Calculate the actual size of just this service
    api::ProtoSize service_sizer;
    service_resp.calculate_size(service_sizer);
    size_t service_size = service_sizer.get_size() + 1;  // +1 for field tag

    // Check if adding this service would exceed the limit
    if (current_size + service_size > MAX_PACKET_SIZE) {
      // We would go over - pop the last service if we have more than one
      if (resp.services.size() > 1) {
        resp.services.pop_back();
        ESP_LOGD(TAG, "[%d] [%s] Service %d would exceed limit (current: %d + service: %d > %d), sending current batch",
                 this->connection_index_, this->address_str(), this->send_service_, current_size, service_size,
                 MAX_PACKET_SIZE);
        // Don't increment send_service_ - we'll retry this service in next batch
      } else {
        // This single service is too large, but we have to send it anyway
        ESP_LOGV(TAG, "[%d] [%s] Service %d is too large (%d bytes) but sending anyway", this->connection_index_,
                 this->address_str(), this->send_service_, service_size);
        // Increment so we don't get stuck
        this->send_service_++;
      }
      // Send what we have
      break;
    }

    // Now we know we're keeping this service, add its size
    current_size += service_size;
    // Successfully added this service, increment counter
    this->send_service_++;
  }

  // Send the message with dynamically batched services
  api_conn->send_message(resp, api::BluetoothGATTGetServicesResponse::MESSAGE_TYPE);
}

void BluetoothConnection::log_connection_error_(const char *operation, esp_gatt_status_t status) {
  ESP_LOGE(TAG, "[%d] [%s] %s error, status=%d", this->connection_index_, this->address_str(), operation, status);
}

void BluetoothConnection::log_connection_warning_(const char *operation, esp_err_t err) {
  ESP_LOGW(TAG, "[%d] [%s] %s failed, err=%d", this->connection_index_, this->address_str(), operation, err);
}

void BluetoothConnection::log_gatt_not_connected_(const char *action, const char *type) {
  ESP_LOGW(TAG, "[%d] [%s] Cannot %s GATT %s, not connected.", this->connection_index_, this->address_str(), action,
           type);
}

void BluetoothConnection::log_gatt_operation_error_(const char *operation, uint16_t handle, esp_gatt_status_t status) {
  ESP_LOGW(TAG, "[%d] [%s] Error %s for handle 0x%2X, status=%d", this->connection_index_, this->address_str(),
           operation, handle, status);
}

esp_err_t BluetoothConnection::check_and_log_error_(const char *operation, esp_err_t err) {
  if (err != ESP_OK) {
    this->log_connection_warning_(operation, err);
    return err;
  }
  return ESP_OK;
}

bool BluetoothConnection::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  if (!BLEClientBase::gattc_event_handler(event, gattc_if, param))
    return false;

  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      // Don't reset connection yet - wait for CLOSE_EVT to ensure controller has freed resources
      // This prevents race condition where we mark slot as free before controller cleanup is complete
      ESP_LOGD(TAG, "[%d] [%s] Disconnect, reason=0x%02x", this->connection_index_, this->address_str_,
               param->disconnect.reason);
      // Send disconnection notification but don't free the slot yet
      this->proxy_->send_device_connection(this->address_, false, 0, param->disconnect.reason);
      break;
    }
    case ESP_GATTC_CLOSE_EVT: {
      ESP_LOGD(TAG, "[%d] [%s] Close, reason=0x%02x, freeing slot", this->connection_index_, this->address_str_,
               param->close.reason);
      // Now the GATT connection is fully closed and controller resources are freed
      // Safe to mark the connection slot as available
      this->reset_connection_(param->close.reason);
      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
        this->reset_connection_(param->open.status);
      } else if (this->connection_type_ == espbt::ConnectionType::V3_WITH_CACHE) {
        this->proxy_->send_device_connection(this->address_, true, this->mtu_);
        this->proxy_->send_connections_free();
      }
      this->seen_mtu_or_services_ = false;
      break;
    }
    case ESP_GATTC_CFG_MTU_EVT:
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (!this->seen_mtu_or_services_) {
        // We don't know if we will get the MTU or the services first, so
        // only send the device connection true if we have already received
        // the services.
        this->seen_mtu_or_services_ = true;
        break;
      }
      this->proxy_->send_device_connection(this->address_, true, this->mtu_);
      this->proxy_->send_connections_free();
      break;
    }
    case ESP_GATTC_READ_DESCR_EVT:
    case ESP_GATTC_READ_CHAR_EVT: {
      if (param->read.status != ESP_GATT_OK) {
        this->log_gatt_operation_error_("reading char/descriptor", param->read.handle, param->read.status);
        this->proxy_->send_gatt_error(this->address_, param->read.handle, param->read.status);
        break;
      }
      api::BluetoothGATTReadResponse resp;
      resp.address = this->address_;
      resp.handle = param->read.handle;
      resp.set_data(param->read.value, param->read.value_len);
      this->proxy_->get_api_connection()->send_message(resp, api::BluetoothGATTReadResponse::MESSAGE_TYPE);
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        this->log_gatt_operation_error_("writing char/descriptor", param->write.handle, param->write.status);
        this->proxy_->send_gatt_error(this->address_, param->write.handle, param->write.status);
        break;
      }
      api::BluetoothGATTWriteResponse resp;
      resp.address = this->address_;
      resp.handle = param->write.handle;
      this->proxy_->get_api_connection()->send_message(resp, api::BluetoothGATTWriteResponse::MESSAGE_TYPE);
      break;
    }
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: {
      if (param->unreg_for_notify.status != ESP_GATT_OK) {
        this->log_gatt_operation_error_("unregistering notifications", param->unreg_for_notify.handle,
                                        param->unreg_for_notify.status);
        this->proxy_->send_gatt_error(this->address_, param->unreg_for_notify.handle, param->unreg_for_notify.status);
        break;
      }
      api::BluetoothGATTNotifyResponse resp;
      resp.address = this->address_;
      resp.handle = param->unreg_for_notify.handle;
      this->proxy_->get_api_connection()->send_message(resp, api::BluetoothGATTNotifyResponse::MESSAGE_TYPE);
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        this->log_gatt_operation_error_("registering notifications", param->reg_for_notify.handle,
                                        param->reg_for_notify.status);
        this->proxy_->send_gatt_error(this->address_, param->reg_for_notify.handle, param->reg_for_notify.status);
        break;
      }
      api::BluetoothGATTNotifyResponse resp;
      resp.address = this->address_;
      resp.handle = param->reg_for_notify.handle;
      this->proxy_->get_api_connection()->send_message(resp, api::BluetoothGATTNotifyResponse::MESSAGE_TYPE);
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      ESP_LOGV(TAG, "[%d] [%s] ESP_GATTC_NOTIFY_EVT: handle=0x%2X", this->connection_index_, this->address_str_,
               param->notify.handle);
      api::BluetoothGATTNotifyDataResponse resp;
      resp.address = this->address_;
      resp.handle = param->notify.handle;
      resp.set_data(param->notify.value, param->notify.value_len);
      this->proxy_->get_api_connection()->send_message(resp, api::BluetoothGATTNotifyDataResponse::MESSAGE_TYPE);
      break;
    }
    default:
      break;
  }
  return true;
}

void BluetoothConnection::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  BLEClientBase::gap_event_handler(event, param);

  switch (event) {
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      if (memcmp(param->ble_security.auth_cmpl.bd_addr, this->remote_bda_, 6) != 0)
        break;
      if (param->ble_security.auth_cmpl.success) {
        this->proxy_->send_device_pairing(this->address_, true);
      } else {
        this->proxy_->send_device_pairing(this->address_, false, param->ble_security.auth_cmpl.fail_reason);
      }
      break;
    default:
      break;
  }
}

esp_err_t BluetoothConnection::read_characteristic(uint16_t handle) {
  if (!this->connected()) {
    this->log_gatt_not_connected_("read", "characteristic");
    return ESP_GATT_NOT_CONNECTED;
  }

  ESP_LOGV(TAG, "[%d] [%s] Reading GATT characteristic handle %d", this->connection_index_, this->address_str_, handle);

  esp_err_t err = esp_ble_gattc_read_char(this->gattc_if_, this->conn_id_, handle, ESP_GATT_AUTH_REQ_NONE);
  return this->check_and_log_error_("esp_ble_gattc_read_char", err);
}

esp_err_t BluetoothConnection::write_characteristic(uint16_t handle, const uint8_t *data, size_t length,
                                                    bool response) {
  if (!this->connected()) {
    this->log_gatt_not_connected_("write", "characteristic");
    return ESP_GATT_NOT_CONNECTED;
  }
  ESP_LOGV(TAG, "[%d] [%s] Writing GATT characteristic handle %d", this->connection_index_, this->address_str_, handle);

  // ESP-IDF's API requires a non-const uint8_t* but it doesn't modify the data
  // The BTC layer immediately copies the data to its own buffer (see btc_gattc.c)
  // const_cast is safe here and was previously hidden by a C-style cast
  esp_err_t err =
      esp_ble_gattc_write_char(this->gattc_if_, this->conn_id_, handle, length, const_cast<uint8_t *>(data),
                               response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  return this->check_and_log_error_("esp_ble_gattc_write_char", err);
}

esp_err_t BluetoothConnection::read_descriptor(uint16_t handle) {
  if (!this->connected()) {
    this->log_gatt_not_connected_("read", "descriptor");
    return ESP_GATT_NOT_CONNECTED;
  }
  ESP_LOGV(TAG, "[%d] [%s] Reading GATT descriptor handle %d", this->connection_index_, this->address_str_, handle);

  esp_err_t err = esp_ble_gattc_read_char_descr(this->gattc_if_, this->conn_id_, handle, ESP_GATT_AUTH_REQ_NONE);
  return this->check_and_log_error_("esp_ble_gattc_read_char_descr", err);
}

esp_err_t BluetoothConnection::write_descriptor(uint16_t handle, const uint8_t *data, size_t length, bool response) {
  if (!this->connected()) {
    this->log_gatt_not_connected_("write", "descriptor");
    return ESP_GATT_NOT_CONNECTED;
  }
  ESP_LOGV(TAG, "[%d] [%s] Writing GATT descriptor handle %d", this->connection_index_, this->address_str_, handle);

  // ESP-IDF's API requires a non-const uint8_t* but it doesn't modify the data
  // The BTC layer immediately copies the data to its own buffer (see btc_gattc.c)
  // const_cast is safe here and was previously hidden by a C-style cast
  esp_err_t err = esp_ble_gattc_write_char_descr(
      this->gattc_if_, this->conn_id_, handle, length, const_cast<uint8_t *>(data),
      response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  return this->check_and_log_error_("esp_ble_gattc_write_char_descr", err);
}

esp_err_t BluetoothConnection::notify_characteristic(uint16_t handle, bool enable) {
  if (!this->connected()) {
    this->log_gatt_not_connected_("notify", "characteristic");
    return ESP_GATT_NOT_CONNECTED;
  }

  if (enable) {
    ESP_LOGV(TAG, "[%d] [%s] Registering for GATT characteristic notifications handle %d", this->connection_index_,
             this->address_str_, handle);
    esp_err_t err = esp_ble_gattc_register_for_notify(this->gattc_if_, this->remote_bda_, handle);
    return this->check_and_log_error_("esp_ble_gattc_register_for_notify", err);
  }

  ESP_LOGV(TAG, "[%d] [%s] Unregistering for GATT characteristic notifications handle %d", this->connection_index_,
           this->address_str_, handle);
  esp_err_t err = esp_ble_gattc_unregister_for_notify(this->gattc_if_, this->remote_bda_, handle);
  return this->check_and_log_error_("esp_ble_gattc_unregister_for_notify", err);
}

esp32_ble_tracker::AdvertisementParserType BluetoothConnection::get_advertisement_parser_type() {
  return this->proxy_->get_advertisement_parser_type();
}

}  // namespace esphome::bluetooth_proxy

#endif  // USE_ESP32
