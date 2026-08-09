#include "bluetooth_connection_bluedroid.h"

#if defined(USE_ESP32_BLE) && defined(USE_BLE_GATT_CLIENT)

#include "bluetooth_connection.h"
#include "bluetooth_connection_hub.h"

#include "esphome/components/ble_device_base/ble_client_state.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_bt.h>
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
  if (st == ClientState::IDLE || st == ClientState::DISCONNECTING) {
    return 0;
  }
  if (st == ClientState::DISCOVERED) {
    // Never opened: nothing to close.
    this->set_state_(ClientState::IDLE);
    return 0;
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

ble_device_base::GattServiceTable BluedroidGattClient::get_service_table(uint16_t first_service) {
  ble_device_base::GattServiceTable table{};
  if (this->services_ == nullptr || this->services_released_) {
    return table;
  }
  table.services = this->services_;
  table.service_count = this->service_total_;
  if (first_service >= this->service_total_) {
    // Terminal pump: the wrapper only reads service_count.
    return table;
  }
  if (!this->build_window_(first_service, table)) {
    // Zero-length ranges on a non-empty service trip the wrapper's
    // out-of-bounds abort, so a walk failure can never stream a truncated
    // database as authoritative.
    table.characteristics = nullptr;
    table.descriptors = nullptr;
    table.characteristic_count = 0;
    table.descriptor_count = 0;
  }
  return table;
}

void BluedroidGattClient::release_services() {
  RAMAllocator<uint8_t> allocator;
  if (this->services_ != nullptr) {
    allocator.deallocate(reinterpret_cast<uint8_t *>(this->services_), 0);
    this->services_ = nullptr;
  }
  if (this->window_ != nullptr) {
    allocator.deallocate(this->window_, 0);
    this->window_ = nullptr;
    this->window_cap_ = 0;
  }
  if (this->elems_ != nullptr) {
    allocator.deallocate(this->elems_, 0);
    this->elems_ = nullptr;
    this->elems_cap_ = 0;
  }
  this->service_total_ = 0;
#ifndef CONFIG_BT_GATTC_CACHE_NVS_FLASH
  // Only the cache clean makes the stack's database unsafe to walk.
  this->services_released_ = true;
  esp_ble_gattc_cache_clean(this->remote_bda_);
#endif
}

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
  if (this->listener_ != nullptr) {
    this->listener_->on_connection_state(connected, this->mtu_, error);
  }
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

// ---- table materialization ----

uint8_t *BluedroidGattClient::ensure_capacity_(uint8_t *&buf, size_t &cap, size_t needed) {
  if (needed <= cap) {
    return buf;
  }
  RAMAllocator<uint8_t> allocator;
  if (buf != nullptr) {
    allocator.deallocate(buf, 0);
  }
  buf = allocator.allocate(needed);
  cap = buf != nullptr ? needed : 0;
  return buf;
}

bool BluedroidGattClient::materialize_services_() {
  if (this->services_released_) {
    ESP_LOGW(TAG, "[%d] Services released, cannot walk the GATT cache", this->connection_index_);
    return false;
  }
  uint16_t primary = 0;
  uint16_t secondary = 0;
  esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_PRIMARY_SERVICE, 0x0001, 0xFFFF, 0,
                               &primary);
  esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_SECONDARY_SERVICE, 0x0001, 0xFFFF, 0,
                               &secondary);
  const uint16_t total = primary + secondary;
  if (total == 0) {
    this->service_total_ = 0;
    return true;
  }
  RAMAllocator<uint8_t> allocator;
  this->services_ = reinterpret_cast<ble_device_base::GattService *>(
      allocator.allocate(total * sizeof(ble_device_base::GattService)));
  if (this->services_ == nullptr) {
    ESP_LOGE(TAG, "[%d] Service list allocation failed", this->connection_index_);
    return false;
  }
  uint16_t filled = 0;
  for (uint16_t s = 0; s < total; s++) {
    esp_gattc_service_elem_t service_result;
    uint16_t count = 1;
    if (esp_ble_gattc_get_service(this->gattc_if_, this->conn_id_, nullptr, &service_result, &count, s) !=
            ESP_GATT_OK ||
        count == 0) {
      break;
    }
    auto &service = this->services_[filled];
    service.uuid = esp32_ble_tracker::ESPBTUUID::from_uuid(service_result.uuid);
    service.start_handle = service_result.start_handle;
    service.end_handle = service_result.end_handle;
    service.first_characteristic = 0;
    uint16_t char_count = 0;
    esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_CHARACTERISTIC,
                                 service_result.start_handle, service_result.end_handle, 0, &char_count);
    service.characteristic_count = char_count;
    filled++;
  }
  this->service_total_ = filled;
  return true;
}

bool BluedroidGattClient::build_window_(uint16_t first_service, ble_device_base::GattServiceTable &out) {
  if (this->services_released_) {
    return false;
  }
  // Cover at least the services the wrapper can admit into one batch. Sizing
  // with the smaller (efficient-UUID) estimate makes the window a superset of
  // the admitted set in either UUID mode.
  uint16_t window_end = first_service;
  size_t est = 0;
  uint16_t chars_total = 0;
  uint16_t descs_bound = 0;
  uint16_t elems_max = 0;
  while (window_end < this->service_total_) {
    const auto &svc = this->services_[window_end];
    uint16_t n_attrs = 0;
    if (esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_ALL, svc.start_handle, svc.end_handle,
                                     0, &n_attrs) != ESP_GATT_OK) {
      return false;
    }
    chars_total += svc.characteristic_count;
    if (n_attrs > 1 + svc.characteristic_count) {
      descs_bound += n_attrs - 1 - svc.characteristic_count;
    }
    if (n_attrs > elems_max) {
      elems_max = n_attrs;
    }
    est += estimate_service_size(svc.characteristic_count, true);
    window_end++;
    if (est > MAX_PACKET_SIZE) {
      break;
    }
  }

  const size_t chr_bytes = chars_total * sizeof(ble_device_base::GattCharacteristic);
  if (this->ensure_capacity_(this->window_, this->window_cap_,
                             chr_bytes + descs_bound * sizeof(ble_device_base::GattDescriptor)) == nullptr ||
      this->ensure_capacity_(this->elems_, this->elems_cap_, elems_max * sizeof(esp_gattc_db_elem_t)) == nullptr) {
    ESP_LOGE(TAG, "[%d] Service window allocation failed", this->connection_index_);
    return false;
  }
  auto *chars = reinterpret_cast<ble_device_base::GattCharacteristic *>(this->window_);
  auto *descs = reinterpret_cast<ble_device_base::GattDescriptor *>(this->window_ + chr_bytes);
  auto *elems = reinterpret_cast<esp_gattc_db_elem_t *>(this->elems_);

  uint16_t chr_index = 0;
  uint16_t dsc_index = 0;
  for (uint16_t s = first_service; s < window_end; s++) {
    auto &svc = this->services_[s];
    svc.first_characteristic = chr_index;
    uint16_t filled_chars = 0;
    uint16_t count = elems_max;
    if (esp_ble_gattc_get_db(this->gattc_if_, this->conn_id_, svc.start_handle, svc.end_handle, elems, &count) !=
        ESP_GATT_OK) {
      return false;
    }
    // The snapshot is handle-ordered: descriptors follow their characteristic.
    ble_device_base::GattCharacteristic *chr = nullptr;
    for (uint16_t i = 0; i < count; i++) {
      const auto &elem = elems[i];
      if (elem.type == ESP_GATT_DB_CHARACTERISTIC) {
        // Bounded by the pass-1 count: a misbehaving peripheral can return
        // more entries than it reported.
        if (filled_chars >= svc.characteristic_count || chr_index >= chars_total) {
          break;
        }
        chr = &chars[chr_index++];
        filled_chars++;
        chr->uuid = esp32_ble_tracker::ESPBTUUID::from_uuid(elem.uuid);
        chr->value_handle = elem.attribute_handle;
        chr->end_handle = elem.attribute_handle;
        chr->properties = elem.properties;
        chr->first_descriptor = dsc_index;
        chr->descriptor_count = 0;
      } else if (elem.type == ESP_GATT_DB_DESCRIPTOR && chr != nullptr && dsc_index < descs_bound) {
        descs[dsc_index].uuid = esp32_ble_tracker::ESPBTUUID::from_uuid(elem.uuid);
        descs[dsc_index].handle = elem.attribute_handle;
        dsc_index++;
        chr->descriptor_count++;
      }
    }
    svc.characteristic_count = filled_chars;
  }

  out.characteristics = chars;
  out.descriptors = descs;
  out.characteristic_count = chr_index;
  out.descriptor_count = dsc_index;
  return true;
}

void BluedroidGattClient::handle_search_cmpl_() {
  // Step down from the fast discovery params.
  this->update_conn_params_(MEDIUM_MIN_CONN_INTERVAL, MEDIUM_MAX_CONN_INTERVAL, 0, MEDIUM_CONN_TIMEOUT, "medium");
  bool ok = this->materialize_services_();
  if (this->listener_ != nullptr) {
    this->listener_->on_service_discovery_done(ok ? 0 : ble_device_base::GATT_ERR_NO_MEMORY);
  }
}

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
  if (this->shim_.disconnect_scheduled()) {
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
  // Passive disconnect: report now, but wait for CLOSE_EVT before going IDLE -
  // reconnecting earlier makes the controller reject with 133 or assert.
  this->release_services();
  this->set_disconnecting_();
  this->report_connection_state_(false, param->disconnect.reason);
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
      // The wrapper frees the slot on this final report; after a passive
      // disconnect this is the second connected=false, matching the previous
      // esp32 behavior (report at DISCONNECT, slot free at CLOSE).
      this->report_connection_state_(false, param->close.reason);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (this->conn_id_ != param->search_cmpl.conn_id)
        return false;
      ESP_LOGI(TAG, "[%d] Service discovery complete", this->connection_index_);
      this->set_state_(ClientState::ESTABLISHED);
      this->handle_search_cmpl_();
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT:
    case ESP_GATTC_READ_DESCR_EVT: {
      if (this->conn_id_ != param->read.conn_id)
        return false;
      if (this->listener_ != nullptr) {
        bool ok = param->read.status == ESP_GATT_OK;
        this->listener_->on_read_result(param->read.handle, ok ? param->read.value : nullptr,
                                        ok ? param->read.value_len : 0, ok ? 0 : param->read.status);
      }
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (this->conn_id_ != param->write.conn_id)
        return false;
      if (this->listener_ != nullptr) {
        this->listener_->on_write_result(param->write.handle,
                                         param->write.status == ESP_GATT_OK ? 0 : param->write.status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (this->listener_ != nullptr) {
        this->listener_->on_notify_state(
            param->reg_for_notify.handle, true,
            param->reg_for_notify.status == ESP_GATT_OK ? 0 : param->reg_for_notify.status);
      }
      break;
    }
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: {
      if (this->listener_ != nullptr) {
        this->listener_->on_notify_state(
            param->unreg_for_notify.handle, false,
            param->unreg_for_notify.status == ESP_GATT_OK ? 0 : param->unreg_for_notify.status);
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (this->conn_id_ != param->notify.conn_id)
        return false;
      ESP_LOGV(TAG, "[%d] NOTIFY_EVT handle=0x%2X", this->connection_index_, param->notify.handle);
      if (this->listener_ != nullptr) {
        this->listener_->on_notify_data(param->notify.handle, param->notify.value, param->notify.value_len);
      }
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
      if (this->listener_ != nullptr) {
        this->listener_->on_pairing_result(
            param->ble_security.auth_cmpl.success ? 0 : param->ble_security.auth_cmpl.fail_reason);
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace esphome::bluetooth_connection

#endif  // USE_ESP32_BLE && USE_BLE_GATT_CLIENT
