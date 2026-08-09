#include "bluetooth_connection_bluedroid.h"

#if defined(USE_ESP32_BLE) && defined(USE_BLE_GATT_CLIENT)

// The in-place streamer serves the proxy's service-discovery API; backend-only
// builds compile without the proxy headers or the streamer.
#ifdef USE_BLUETOOTH_PROXY
#include "bluetooth_connection.h"
#include "bluetooth_connection_hub.h"

#include "esphome/components/bluetooth_proxy/bluetooth_proxy.h"
#endif

#include "esphome/components/ble_device_base/ble_client_state.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_gatt_common_api.h>

#include <cstring>

namespace esphome::bluetooth_connection {

static const char *const TAG = "bluetooth_connection.bluedroid";

using ble_device_base::FAST_CONN_TIMEOUT;
using ble_device_base::FAST_MAX_CONN_INTERVAL;
using ble_device_base::FAST_MIN_CONN_INTERVAL;
using ble_device_base::MEDIUM_CONN_TIMEOUT;
using ble_device_base::MEDIUM_MAX_CONN_INTERVAL;
using ble_device_base::MEDIUM_MIN_CONN_INTERVAL;
using esp32_ble_tracker::ClientState;
using esp32_ble_tracker::ConnectionType;

static constexpr uint16_t UNSET_CONN_ID = 0xFFFF;

// ---- shim forwarders ----

bool BluedroidTrackerShim::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                               esp_ble_gattc_cb_param_t *param) {
  return this->engine_->handle_gattc_event_(event, gattc_if, param);
}
void BluedroidTrackerShim::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  this->engine_->handle_gap_event_(event, param);
}
void BluedroidTrackerShim::connect() { this->engine_->tracker_connect_(); }
void BluedroidTrackerShim::disconnect() { this->engine_->disconnect(); }

// ---- component ----

void BluedroidGattClient::setup() {
  static uint8_t connection_index = 0;
  this->connection_index_ = connection_index++;
}

void BluedroidGattClient::loop() {
  if (!esp32_ble::global_ble->is_active()) {
    // Stack down: re-register the app on the next enable.
    this->set_state_(ClientState::INIT);
    return;
  }
  auto st = this->state_();
  if (st == ClientState::INIT) {
    auto ret = esp_ble_gattc_app_register(this->shim_.app_id);
    if (ret) {
      ESP_LOGE(TAG, "gattc app register failed: app_id=%d code=%d", this->shim_.app_id, ret);
      this->mark_failed();
    }
    // Do not wait for REG_EVT; a dropped event must not wedge the slot.
    this->set_state_(ClientState::IDLE);
  } else if (st == ClientState::IDLE) {
    // The loop only drives the bootstrap and the disconnect safety timeout.
    this->disable_loop();
  } else if (st == ClientState::DISCONNECTING &&
             millis() - this->disconnecting_started_ > ble_device_base::GATT_DISCONNECT_TIMEOUT_MS) {
    ESP_LOGE(TAG, "[%d] Timeout waiting for CLOSE_EVT, forcing IDLE", this->connection_index_);
    // Release before idling: unconditional disconnect does not release, and a
    // lost CLOSE/DISCONNECT would otherwise leak the table and the cache.
    this->release_services();
    this->set_idle_();
    this->report_connection_state_(false, ESP_GATT_CONN_TIMEOUT);
  }
}

void BluedroidGattClient::dump_config() {
  ESP_LOGCONFIG(TAG, "Bluedroid GATT client %d", this->connection_index_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Registration failed; if the error was ESP_GATT_NO_RESOURCES, reduce the connection slots");
  }
}

// ---- contract ops ----

int BluedroidGattClient::connect(uint64_t address, uint8_t addr_type) {
  // Refuse anything but a fully idle slot. Clobbering DISCONNECTING with
  // DISCOVERED would let the tracker open a new link while the old one is
  // still closing - the stale CLOSE_EVT then tears the new attempt down.
  if (this->state_() != ClientState::IDLE) {
    ESP_LOGW(TAG, "[%d] Connect rejected, slot busy", this->connection_index_);
    return ESP_GATT_BUSY;
  }
  ble_device_base::uint64_to_mac_msb_first(address, this->remote_bda_);
  this->remote_addr_type_ = addr_type;
  // Hand the request to the tracker's promote loop: it stops the scan, raises
  // coex, and calls tracker_connect_() - the tracker owns connect timing here.
  this->set_state_(ClientState::DISCOVERED);
  return 0;
}

void BluedroidGattClient::tracker_connect_() {
  auto st = this->state_();
  if (st == ClientState::CONNECTING || st == ClientState::CONNECTED || st == ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%d] Connection already in progress", this->connection_index_);
    return;
  }
  if (st == ClientState::DISCONNECTING) {
    ESP_LOGW(TAG, "[%d] Cannot connect, still waiting for CLOSE_EVT", this->connection_index_);
    return;
  }
  ESP_LOGI(TAG, "[%d] 0x%02x Connecting", this->connection_index_, this->remote_addr_type_);
  this->services_released_ = false;
  this->seen_mtu_ = false;
  this->enable_loop();
  this->set_state_(ClientState::CONNECTING);
  if (this->connection_type_ == ConnectionType::V3_WITHOUT_CACHE) {
    // Fast params for the discovery phase; stepped down at SEARCH_CMPL.
    esp_ble_gap_set_prefer_conn_params(this->remote_bda_, FAST_MIN_CONN_INTERVAL, FAST_MAX_CONN_INTERVAL, 0,
                                       FAST_CONN_TIMEOUT);
  } else {
    esp_ble_gap_set_prefer_conn_params(this->remote_bda_, MEDIUM_MIN_CONN_INTERVAL, MEDIUM_MAX_CONN_INTERVAL, 0,
                                       MEDIUM_CONN_TIMEOUT);
  }
  auto ret = esp_ble_gattc_open(this->gattc_if_, this->remote_bda_,
                                static_cast<esp_ble_addr_type_t>(this->remote_addr_type_), true);
  if (ret) {
    this->log_gattc_warning_("esp_ble_gattc_open", ret);
    // CONNECT_EVT never fired, so conn_id_ is legitimately unset: plain IDLE.
    this->set_state_(ClientState::IDLE);
    this->report_connection_state_(false, ret);
  }
}

int BluedroidGattClient::disconnect() {
  auto st = this->state_();
  if (st == ClientState::DISCONNECTING) {
    return 0;
  }
  // Nothing was opened, so no completion event will follow: report
  // not-connected and the hub frees the slot at once (rp2 convention).
  if (st == ClientState::IDLE) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  if (st == ClientState::DISCOVERED) {
    // Parked for the tracker promote loop, never opened.
    this->set_state_(ClientState::IDLE);
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  if (st == ClientState::CONNECTING || this->conn_id_ == UNSET_CONN_ID) {
    ESP_LOGD(TAG, "[%d] Disconnect scheduled", this->connection_index_);
    this->shim_.schedule_disconnect();
    return 0;
  }
  this->unconditional_disconnect_();
  return 0;
}

void BluedroidGattClient::unconditional_disconnect_() {
  ESP_LOGI(TAG, "[%d] Disconnecting (conn_id: %d)", this->connection_index_, this->conn_id_);
  if (this->conn_id_ == UNSET_CONN_ID) {
    ESP_LOGE(TAG, "[%d] conn id unset, cannot disconnect", this->connection_index_);
    return;
  }
  auto err = esp_ble_gattc_close(this->gattc_if_, this->conn_id_);
  if (err != ESP_OK) {
    // The stack is now in an indeterminate state for this link.
    ESP_LOGE(TAG, "[%d] esp_ble_gattc_close error: %d", this->connection_index_, err);
  }
  this->set_disconnecting_();
}

int BluedroidGattClient::discover_services() {
  if (this->conn_id_ == UNSET_CONN_ID) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  return this->check_and_log_error_("esp_ble_gattc_search_service",
                                    esp_ble_gattc_search_service(this->gattc_if_, this->conn_id_, nullptr));
}

int BluedroidGattClient::read_characteristic(uint16_t handle) {
  return this->check_and_log_error_("esp_ble_gattc_read_char", esp_ble_gattc_read_char(this->gattc_if_, this->conn_id_,
                                                                                       handle, ESP_GATT_AUTH_REQ_NONE));
}

int BluedroidGattClient::write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) {
  // The BTC layer copies the payload immediately, so the const_cast is safe.
  return this->check_and_log_error_(
      "esp_ble_gattc_write_char",
      esp_ble_gattc_write_char(this->gattc_if_, this->conn_id_, handle, len, const_cast<uint8_t *>(data),
                               response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP,
                               ESP_GATT_AUTH_REQ_NONE));
}

int BluedroidGattClient::read_descriptor(uint16_t handle) {
  return this->check_and_log_error_(
      "esp_ble_gattc_read_char_descr",
      esp_ble_gattc_read_char_descr(this->gattc_if_, this->conn_id_, handle, ESP_GATT_AUTH_REQ_NONE));
}

int BluedroidGattClient::write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) {
  return this->check_and_log_error_(
      "esp_ble_gattc_write_char_descr",
      esp_ble_gattc_write_char_descr(this->gattc_if_, this->conn_id_, handle, len, const_cast<uint8_t *>(data),
                                     ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE));
}

int BluedroidGattClient::notify_characteristic(uint16_t handle, bool enable) {
  // Local registration only; the CCCD write is the API client's responsibility.
  if (enable) {
    return this->check_and_log_error_("esp_ble_gattc_register_for_notify",
                                      esp_ble_gattc_register_for_notify(this->gattc_if_, this->remote_bda_, handle));
  }
  return this->check_and_log_error_("esp_ble_gattc_unregister_for_notify",
                                    esp_ble_gattc_unregister_for_notify(this->gattc_if_, this->remote_bda_, handle));
}

int BluedroidGattClient::pair() { return esp_ble_set_encryption(this->remote_bda_, ESP_BLE_SEC_ENCRYPT); }

int BluedroidGattClient::update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                                  uint16_t timeout) {
  return this->update_conn_params_(min_interval, max_interval, latency, timeout, "custom");
}

void BluedroidGattClient::release_services() {
  this->service_total_ = 0;
#ifdef USE_BLE_GATT_SERVICE_TABLE
  this->free_service_table_();
#endif
#ifndef CONFIG_BT_GATTC_CACHE_NVS_FLASH
  // Only the cache clean makes the stack's database unsafe to walk.
  this->services_released_ = true;
  esp_ble_gattc_cache_clean(this->remote_bda_);
#endif
}

#ifdef USE_BLE_GATT_SERVICE_TABLE
ble_device_base::GattServiceTable BluedroidGattClient::get_service_table() {
  if (this->table_storage_ == nullptr &&
      (this->services_released_ || this->service_total_ == 0 || !this->build_service_table_())) {
    return {};
  }
  return this->table_view_();
}

// The view is carved from the storage block and the counts on each call
// (a cold path) rather than cached, saving a per-instance table member.
ble_device_base::GattServiceTable BluedroidGattClient::table_view_() const {
  size_t svc_bytes = this->service_total_ * sizeof(ble_device_base::GattService);
  size_t char_bytes = this->table_char_total_ * sizeof(ble_device_base::GattCharacteristic);
  return {reinterpret_cast<const ble_device_base::GattService *>(this->table_storage_),
          reinterpret_cast<const ble_device_base::GattCharacteristic *>(this->table_storage_ + svc_bytes),
          reinterpret_cast<const ble_device_base::GattDescriptor *>(this->table_storage_ + svc_bytes + char_bytes),
          this->service_total_,
          this->table_char_total_,
          this->table_desc_total_};
}

void BluedroidGattClient::free_service_table_() {
  if (this->table_storage_ == nullptr) {
    return;
  }
  RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::ALLOC_INTERNAL);
  allocator.deallocate(this->table_storage_, 0);
  this->table_storage_ = nullptr;
  this->table_char_total_ = 0;
  this->table_desc_total_ = 0;
}

template<typename ServiceFn, typename CharFn, typename DescFn>
bool BluedroidGattClient::walk_database_(ServiceFn &&on_service, CharFn &&on_char, DescFn &&on_desc) {
  // Shared enumeration for both table-build passes: an identical walk order
  // is what lets the counting pass size the block the filling pass fills.
  // INVALID_OFFSET/NOT_FOUND mean end-of-range; anything else is a failure.
  for (uint16_t s = 0; s < this->service_total_; s++) {
    esp_gattc_service_elem_t svc;
    uint16_t svc_count = 1;
    if (esp_ble_gattc_get_service(this->gattc_if_, this->conn_id_, nullptr, &svc, &svc_count, s) != ESP_GATT_OK ||
        svc_count == 0) {
      return false;
    }
    if (!on_service(s, svc)) {
      return false;
    }
    uint16_t svc_chars = 0;
    if (esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_CHARACTERISTIC, svc.start_handle,
                                     svc.end_handle, 0, &svc_chars) != ESP_GATT_OK) {
      return false;
    }
    for (uint16_t c = 0; c < svc_chars; c++) {
      esp_gattc_char_elem_t chr;
      uint16_t char_count = 1;
      auto status = esp_ble_gattc_get_all_char(this->gattc_if_, this->conn_id_, svc.start_handle, svc.end_handle, &chr,
                                               &char_count, c);
      if (status == ESP_GATT_INVALID_OFFSET || status == ESP_GATT_NOT_FOUND) {
        break;
      }
      if (status != ESP_GATT_OK || char_count == 0) {
        return false;
      }
      if (!on_char(svc, chr)) {
        return false;
      }
      for (uint16_t d = 0;; d++) {
        esp_gattc_descr_elem_t desc;
        uint16_t desc_count = 1;
        auto desc_status =
            esp_ble_gattc_get_all_descr(this->gattc_if_, this->conn_id_, chr.char_handle, &desc, &desc_count, d);
        if (desc_status == ESP_GATT_INVALID_OFFSET || desc_status == ESP_GATT_NOT_FOUND) {
          break;
        }
        if (desc_status != ESP_GATT_OK || desc_count == 0) {
          return false;
        }
        if (!on_desc(chr, desc)) {
          return false;
        }
      }
    }
  }
  return true;
}

bool BluedroidGattClient::build_service_table_() {
  // Pass 1: count, so one exact-size block holds the whole table.
  uint16_t char_total = 0;
  uint16_t desc_total = 0;
  bool counted = this->walk_database_([](uint16_t, const esp_gattc_service_elem_t &) { return true; },
                                      [&](const esp_gattc_service_elem_t &, const esp_gattc_char_elem_t &) {
                                        char_total++;
                                        return true;
                                      },
                                      [&](const esp_gattc_char_elem_t &, const esp_gattc_descr_elem_t &) {
                                        desc_total++;
                                        return true;
                                      });
  if (!counted) {
    return false;
  }

  // The arrays share one block; carving stays aligned because each struct's
  // strictest member is the UUID and array sizes are multiples of it.
  size_t svc_bytes = this->service_total_ * sizeof(ble_device_base::GattService);
  size_t char_bytes = char_total * sizeof(ble_device_base::GattCharacteristic);
  size_t total_bytes = svc_bytes + char_bytes + desc_total * sizeof(ble_device_base::GattDescriptor);
  RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::ALLOC_INTERNAL);
  this->table_storage_ = allocator.allocate(total_bytes);
  if (this->table_storage_ == nullptr) {
    ESP_LOGW(TAG, "[%d] Service table allocation failed (%u bytes)", this->connection_index_,
             static_cast<unsigned>(total_bytes));
    return false;
  }
  auto *services = reinterpret_cast<ble_device_base::GattService *>(this->table_storage_);
  auto *characteristics = reinterpret_cast<ble_device_base::GattCharacteristic *>(this->table_storage_ + svc_bytes);
  auto *descriptors =
      reinterpret_cast<ble_device_base::GattDescriptor *>(this->table_storage_ + svc_bytes + char_bytes);

  // Pass 2: fill, bounded by the pass-1 totals. A bound trip or a shortfall
  // means the cached database changed between the passes; fail the build
  // rather than serve an inconsistent table (the consumer retries).
  uint16_t char_index = 0;
  uint16_t desc_index = 0;
  ble_device_base::GattService *cur_service = nullptr;
  ble_device_base::GattCharacteristic *cur_char = nullptr;
  bool filled = this->walk_database_(
      [&](uint16_t s, const esp_gattc_service_elem_t &svc) {
        cur_service = &services[s];
        cur_service->uuid = ble_device_base::ESPBTUUID::from_uuid(svc.uuid);
        cur_service->start_handle = svc.start_handle;
        cur_service->end_handle = svc.end_handle;
        cur_service->first_characteristic = char_index;
        cur_service->characteristic_count = 0;
        return true;
      },
      [&](const esp_gattc_service_elem_t &svc, const esp_gattc_char_elem_t &chr) {
        if (char_index >= char_total) {
          return false;
        }
        cur_char = &characteristics[char_index++];
        cur_char->uuid = ble_device_base::ESPBTUUID::from_uuid(chr.uuid);
        cur_char->value_handle = chr.char_handle;
        // Bluedroid addresses descriptors by characteristic handle, so the
        // table's end_handle only needs the service-bounded upper bound.
        cur_char->end_handle = svc.end_handle;
        cur_char->properties = chr.properties;
        cur_char->first_descriptor = desc_index;
        cur_char->descriptor_count = 0;
        cur_service->characteristic_count++;
        return true;
      },
      [&](const esp_gattc_char_elem_t &, const esp_gattc_descr_elem_t &desc) {
        if (desc_index >= desc_total) {
          return false;
        }
        descriptors[desc_index].uuid = ble_device_base::ESPBTUUID::from_uuid(desc.uuid);
        descriptors[desc_index].handle = desc.handle;
        desc_index++;
        cur_char->descriptor_count++;
        return true;
      });
  if (!filled || char_index != char_total || desc_index != desc_total) {
    this->free_service_table_();
    return false;
  }
  this->table_char_total_ = char_total;
  this->table_desc_total_ = desc_total;
  return true;
}
#endif  // USE_BLE_GATT_SERVICE_TABLE

// ---- internals ----

bool BluedroidGattClient::check_addr_(const esp_bd_addr_t &addr) const {
  return memcmp(addr, this->remote_bda_, sizeof(esp_bd_addr_t)) == 0;
}

void BluedroidGattClient::set_idle_() {
  this->set_state_(ClientState::IDLE);
  this->conn_id_ = UNSET_CONN_ID;
}

void BluedroidGattClient::set_disconnecting_() {
  this->disconnecting_started_ = millis();
  this->set_state_(ClientState::DISCONNECTING);
  // The loop may be disabled while idle; the safety timeout needs it.
  this->enable_loop();
}

void BluedroidGattClient::report_connection_state_(bool connected, int error) {
  this->listener_->on_connection_state(connected, this->mtu_, error);
}

esp_err_t BluedroidGattClient::update_conn_params_(uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                                   uint16_t timeout, const char *param_type) {
  esp_ble_conn_update_params_t conn_params = {{0}};
  memcpy(conn_params.bda, this->remote_bda_, sizeof(esp_bd_addr_t));
  conn_params.min_int = min_interval;
  conn_params.max_int = max_interval;
  conn_params.latency = latency;
  conn_params.timeout = timeout;
  ESP_LOGD(TAG, "[%d] %s conn params", this->connection_index_, param_type);
  return this->check_and_log_error_("esp_ble_gap_update_conn_params", esp_ble_gap_update_conn_params(&conn_params));
}

int BluedroidGattClient::check_and_log_error_(const char *operation, esp_err_t err) {
  if (err != ESP_OK) {
    this->log_gattc_warning_(operation, err);
  }
  return err;
}

void BluedroidGattClient::log_gattc_warning_(const char *operation, int code) {
  ESP_LOGW(TAG, "[%d] %s failed, status=%d", this->connection_index_, operation, code);
}

// ---- service streaming ----

void BluedroidGattClient::handle_search_cmpl_() {
  // Step down from the fast discovery params.
  this->update_conn_params_(MEDIUM_MIN_CONN_INTERVAL, MEDIUM_MAX_CONN_INTERVAL, 0, MEDIUM_CONN_TIMEOUT, "medium");
  uint16_t primary = 0;
  uint16_t secondary = 0;
  auto primary_status = esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_PRIMARY_SERVICE,
                                                     0x0001, 0xFFFF, 0, &primary);
  auto secondary_status = esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_SECONDARY_SERVICE,
                                                       0x0001, 0xFFFF, 0, &secondary);
  if (primary_status != ESP_GATT_OK || secondary_status != ESP_GATT_OK) {
    // A failed count must not become an authoritative empty database - V3
    // clients cache the streamed result permanently.
    auto status = primary_status != ESP_GATT_OK ? primary_status : secondary_status;
    this->log_gattc_warning_("esp_ble_gattc_get_attr_count", status);
    this->listener_->on_service_discovery_done(status);
    return;
  }
  this->service_total_ = primary + secondary;
  this->listener_->on_service_discovery_done(0);
}

#ifdef USE_BLUETOOTH_PROXY
void BluedroidGattClient::stream_service_batch(BluetoothConnection &conn) {
  if (this->services_released_ || conn.send_service_ >= this->service_total_) {
    conn.send_service_ = DONE_SENDING_SERVICES;
    conn.proxy_->send_gatt_services_done(conn.address_);
    this->release_services();
    return;
  }

  // The subscriber vanished mid-stream: park the cursor at done WITHOUT
  // sending services-done (a resubscribing client gets silence and its 30 s
  // timeout, never an authoritative partial list).
  auto *api_conn = conn.proxy_->get_api_connection();
  if (api_conn == nullptr) {
    ESP_LOGW(TAG, "[%d] [%s] API connection lost while streaming services", conn.connection_index_, conn.address_str_);
    conn.send_service_ = DONE_SENDING_SERVICES;
    this->release_services();
    return;
  }

  bool use_efficient_uuids = conn.proxy_->client_supports_efficient_uuids();
  api::BluetoothGATTGetServicesResponse resp;
  resp.address = conn.address_;
  size_t current_size = resp.calculate_size();
  int16_t batch_start = conn.send_service_;

  while (conn.send_service_ < this->service_total_) {
    esp_gattc_service_elem_t service_result;
    uint16_t svc_count = 1;
    if (esp_ble_gattc_get_service(this->gattc_if_, this->conn_id_, nullptr, &service_result, &svc_count,
                                  conn.send_service_) != ESP_GATT_OK ||
        svc_count == 0) {
      ESP_LOGE(TAG, "[%d] [%s] Service walk failed (service %d), aborting stream", conn.connection_index_,
               conn.address_str_, conn.send_service_);
      conn.send_service_ = DONE_SENDING_SERVICES;
      return;
    }
    uint16_t total_char_count = 0;
    if (esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_CHARACTERISTIC,
                                     service_result.start_handle, service_result.end_handle, 0,
                                     &total_char_count) != ESP_GATT_OK) {
      conn.send_service_ = DONE_SENDING_SERVICES;
      return;
    }

    // If this service likely won't fit, send the current batch first.
    size_t estimated_size = estimate_service_size(total_char_count, use_efficient_uuids);
    if (!resp.services.empty() && current_size + estimated_size > MAX_PACKET_SIZE) {
      break;
    }

    resp.services.emplace_back();
    auto &service_resp = resp.services.back();
    fill_gatt_uuid(service_resp.uuid, service_resp.short_uuid,
                   ble_device_base::ESPBTUUID::from_uuid(service_result.uuid), use_efficient_uuids);
    service_resp.handle = service_result.start_handle;

    if (total_char_count > 0) {
      service_resp.characteristics.init(total_char_count);
      uint16_t char_offset = 0;
      esp_gattc_char_elem_t char_result;
      // Bounded by the count query: a misbehaving peripheral can make the
      // enumeration return more entries than it reported.
      while (char_offset < total_char_count) {
        uint16_t cc = 1;
        auto char_status = esp_ble_gattc_get_all_char(this->gattc_if_, this->conn_id_, service_result.start_handle,
                                                      service_result.end_handle, &char_result, &cc, char_offset);
        if (char_status == ESP_GATT_INVALID_OFFSET || char_status == ESP_GATT_NOT_FOUND) {
          break;
        }
        if (char_status != ESP_GATT_OK || cc == 0) {
          if (char_status != ESP_GATT_OK) {
            this->log_gattc_warning_("esp_ble_gattc_get_all_char", char_status);
            conn.send_service_ = DONE_SENDING_SERVICES;
            return;
          }
          break;
        }
        service_resp.characteristics.emplace_back();
        auto &characteristic_resp = service_resp.characteristics.back();
        fill_gatt_uuid(characteristic_resp.uuid, characteristic_resp.short_uuid,
                       ble_device_base::ESPBTUUID::from_uuid(char_result.uuid), use_efficient_uuids);
        characteristic_resp.handle = char_result.char_handle;
        characteristic_resp.properties = char_result.properties;

        uint16_t total_desc_count = 0;
        auto desc_count_status = esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_DESCRIPTOR,
                                                              0, 0, char_result.char_handle, &total_desc_count);
        if (desc_count_status != ESP_GATT_OK) {
          // Abort rather than stream the characteristic descriptor-less: a
          // missing CCCD in a cached database breaks notifications for good.
          this->log_gattc_warning_("esp_ble_gattc_get_attr_count", desc_count_status);
          conn.send_service_ = DONE_SENDING_SERVICES;
          return;
        }
        if (total_desc_count > 0) {
          characteristic_resp.descriptors.init(total_desc_count);
          uint16_t desc_offset = 0;
          esp_gattc_descr_elem_t desc_result;
          while (desc_offset < total_desc_count) {
            uint16_t dc = 1;
            auto desc_status = esp_ble_gattc_get_all_descr(this->gattc_if_, this->conn_id_, char_result.char_handle,
                                                           &desc_result, &dc, desc_offset);
            if (desc_status == ESP_GATT_INVALID_OFFSET || desc_status == ESP_GATT_NOT_FOUND) {
              break;
            }
            if (desc_status != ESP_GATT_OK || dc == 0) {
              if (desc_status != ESP_GATT_OK) {
                this->log_gattc_warning_("esp_ble_gattc_get_all_descr", desc_status);
                conn.send_service_ = DONE_SENDING_SERVICES;
                return;
              }
              break;
            }
            characteristic_resp.descriptors.emplace_back();
            auto &descriptor_resp = characteristic_resp.descriptors.back();
            fill_gatt_uuid(descriptor_resp.uuid, descriptor_resp.short_uuid,
                           ble_device_base::ESPBTUUID::from_uuid(desc_result.uuid), use_efficient_uuids);
            descriptor_resp.handle = desc_result.handle;
            desc_offset++;
          }
        }
        char_offset++;
      }
    }

    if (close_service_batch(resp, current_size, conn.send_service_, conn.connection_index_, conn.address_str_) !=
        BatchClose::CONTINUE) {
      break;
    }
  }

  // On a failed send, rewind the cursor so the batch is retried instead of
  // silently skipped.
  if (!api_conn->send_message(resp)) {
    ESP_LOGW(TAG, "[%d] [%s] Failed to send service batch, retrying", conn.connection_index_, conn.address_str_);
    conn.send_service_ = batch_start;
  }
}
#endif  // USE_BLUETOOTH_PROXY

// ---- events ----

void BluedroidGattClient::handle_open_evt_(esp_ble_gattc_cb_param_t *param) {
  auto st = this->state_();
  if (st == ClientState::IDLE) {
    // IDF can deliver OPEN_EVT after esp_ble_gattc_open already returned an
    // error and the slot went IDLE; do not resurrect it.
    ESP_LOGD(TAG, "[%d] OPEN_EVT in IDLE state (status=%d), ignoring", this->connection_index_, param->open.status);
    return;
  }
  if (st != ClientState::CONNECTING) {
    ESP_LOGE(TAG, "[%d] OPEN_EVT in unexpected state", this->connection_index_);
  }
  if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
    this->log_gattc_warning_("Connection open", param->open.status);
    // Never established, CLOSE_EVT may not follow.
    this->set_idle_();
    this->report_connection_state_(false, param->open.status);
    return;
  }
  if (this->shim_.disconnect_pending()) {
    // Earliest point conn_id_ exists; keep it set so CLOSE_EVT still matches.
    this->unconditional_disconnect_();
    return;
  }
  this->set_state_(ClientState::CONNECTED);
  ESP_LOGI(TAG, "[%d] Connection open", this->connection_index_);
  if (this->connection_type_ == ConnectionType::V3_WITH_CACHE) {
    this->set_state_(ClientState::ESTABLISHED);
    // No discovery phase: report immediately; the MTU report below is
    // suppressed by seen_mtu_ (HA tolerates a post-connect MTU of 23 here,
    // matching the previous esp32 behavior).
    this->seen_mtu_ = true;
    this->report_connection_state_(true, 0);
    // Settled: only the disconnect safety net needs the loop, and
    // set_disconnecting_() re-enables it.
    this->disable_loop();
  }
}

void BluedroidGattClient::handle_disconnect_evt_(esp_ble_gattc_cb_param_t *param) {
  if (param->disconnect.reason == ESP_GATT_CONN_TERMINATE_PEER_USER && this->state_() == ClientState::CONNECTED) {
    ESP_LOGW(TAG, "[%d] Remote closed during discovery", this->connection_index_);
  } else {
    ESP_LOGD(TAG, "[%d] DISCONNECT_EVT reason=0x%02x", this->connection_index_, param->disconnect.reason);
  }
  if (this->state_() == ClientState::IDLE) {
    // Active close delivers CLOSE_EVT first; never walk back to DISCONNECTING.
    return;
  }
  // Passive disconnect: wait for CLOSE_EVT before going IDLE (reconnecting
  // earlier makes the controller reject with 133 or assert) and before
  // reporting - the wrapper frees the slot on the report, and a freed slot
  // invites a reconnect into the still-closing link.
  this->release_services();
  this->set_disconnecting_();
}

bool BluedroidGattClient::handle_gattc_event_(esp_gattc_cb_event_t event, esp_gatt_if_t esp_gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  if (event == ESP_GATTC_REG_EVT && this->shim_.app_id != param->reg.app_id)
    return false;
  if (event != ESP_GATTC_REG_EVT && esp_gattc_if != ESP_GATT_IF_NONE && esp_gattc_if != this->gattc_if_)
    return false;

  switch (event) {
    case ESP_GATTC_REG_EVT: {
      if (param->reg.status == ESP_GATT_OK) {
        this->gattc_if_ = esp_gattc_if;
      } else {
        ESP_LOGE(TAG, "[%d] gattc app registration failed, status=%d", this->connection_index_, param->reg.status);
        this->mark_failed();
      }
      break;
    }
    case ESP_GATTC_CONNECT_EVT: {
      if (!this->check_addr_(param->connect.remote_bda))
        return false;
      this->conn_id_ = param->connect.conn_id;
      // MTU request here rather than OPEN_EVT, matching the IDF examples.
      auto ret = esp_ble_gattc_send_mtu_req(this->gattc_if_, param->connect.conn_id);
      if (ret) {
        this->log_gattc_warning_("esp_ble_gattc_send_mtu_req", ret);
      }
      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      if (!this->check_addr_(param->open.remote_bda))
        return false;
      this->handle_open_evt_(param);
      break;
    }
    case ESP_GATTC_CFG_MTU_EVT: {
      if (this->conn_id_ != param->cfg_mtu.conn_id)
        return false;
      if (param->cfg_mtu.status != ESP_GATT_OK) {
        // Warn only; a disconnect will follow if the link is dead.
        this->log_gattc_warning_("MTU exchange", param->cfg_mtu.status);
      } else {
        this->mtu_ = param->cfg_mtu.mtu;
      }
      if (!this->seen_mtu_) {
        this->seen_mtu_ = true;
        // The connected report waited for the MTU so HA never sees 23.
        this->report_connection_state_(true, 0);
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      if (!this->check_addr_(param->disconnect.remote_bda))
        return false;
      this->handle_disconnect_evt_(param);
      break;
    }
    case ESP_GATTC_CLOSE_EVT: {
      if (this->conn_id_ != param->close.conn_id)
        return false;
      this->release_services();
      this->set_idle_();
      // The one connected=false report: the wrapper frees the slot on it,
      // so it must not fire before the controller finished closing.
      this->report_connection_state_(false, param->close.reason);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (this->conn_id_ != param->search_cmpl.conn_id)
        return false;
      ESP_LOGI(TAG, "[%d] Service discovery complete", this->connection_index_);
      this->set_state_(ClientState::ESTABLISHED);
      this->handle_search_cmpl_();
      // Settled (see the V3_WITH_CACHE arm in handle_open_evt_).
      this->disable_loop();
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT:
    case ESP_GATTC_READ_DESCR_EVT: {
      if (this->conn_id_ != param->read.conn_id)
        return false;
      bool ok = param->read.status == ESP_GATT_OK;
      this->listener_->on_read_result(param->read.handle, ok ? param->read.value : nullptr,
                                      ok ? param->read.value_len : 0, ok ? 0 : param->read.status);
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (this->conn_id_ != param->write.conn_id)
        return false;
      this->listener_->on_write_result(param->write.handle,
                                       param->write.status == ESP_GATT_OK ? 0 : param->write.status);
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->listener_->on_notify_state(param->reg_for_notify.handle, true,
                                       param->reg_for_notify.status == ESP_GATT_OK ? 0 : param->reg_for_notify.status);
      break;
    }
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: {
      this->listener_->on_notify_state(
          param->unreg_for_notify.handle, false,
          param->unreg_for_notify.status == ESP_GATT_OK ? 0 : param->unreg_for_notify.status);
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (this->conn_id_ != param->notify.conn_id)
        return false;
      ESP_LOGV(TAG, "[%d] NOTIFY_EVT handle=0x%2X", this->connection_index_, param->notify.handle);
      this->listener_->on_notify_data(param->notify.handle, param->notify.value, param->notify.value_len);
      break;
    }
    default:
      break;
  }
  return true;
}

void BluedroidGattClient::handle_gap_event_(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_SEC_REQ_EVT: {
      if (!this->check_addr_(param->ble_security.auth_cmpl.bd_addr))
        break;
      // Always accept a server-initiated security request.
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;
    }
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      if (!this->check_addr_(param->ble_security.auth_cmpl.bd_addr))
        break;
      this->listener_->on_pairing_result(
          param->ble_security.auth_cmpl.success ? 0 : param->ble_security.auth_cmpl.fail_reason);
      break;
    }
    default:
      break;
  }
}

}  // namespace esphome::bluetooth_connection

#endif  // USE_ESP32_BLE && USE_BLE_GATT_CLIENT
