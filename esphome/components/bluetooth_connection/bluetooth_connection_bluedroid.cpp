#include "bluetooth_connection_bluedroid.h"

#if defined(USE_ESP32_BLE) && defined(USE_BLE_GATT_CLIENT)

// The in-place streamer serves the proxy's service-discovery API; backend-only
// builds compile without the proxy headers or the streamer.
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
#include "bluetooth_connection.h"
#include "bluetooth_connection_hub.h"

#include "esphome/components/bluetooth_proxy/bluetooth_proxy.h"
#endif

#include "esphome/components/ble_device_base/ble_client_state.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::bluetooth_connection {

static const char *const TAG = "bluetooth_connection";

using ble_device_base::FAST_CONN_TIMEOUT;
using ble_device_base::FAST_MAX_CONN_INTERVAL;
using ble_device_base::FAST_MIN_CONN_INTERVAL;
using ble_device_base::MEDIUM_CONN_TIMEOUT;
using ble_device_base::MEDIUM_MAX_CONN_INTERVAL;
using ble_device_base::MEDIUM_MIN_CONN_INTERVAL;
using esp32_ble_tracker::ClientState;
using esp32_ble_tracker::ConnectionType;

// ---- tracker surface ----

void BluedroidGattClient::connect() { this->tracker_connect_(); }
void BluedroidGattClient::disconnect() { this->gatt_disconnect(); }

// ---- component ----

void BluedroidGattClient::setup() {
  static uint8_t connection_index = 0;
  this->connection_index_ = connection_index++;
}

void BluedroidGattClient::loop() {
  if (!esp32_ble::global_ble->is_active()) {
    // Stack down: no CLOSE_EVT will come. Settle a live link so the consumer
    // frees its slot, then re-register the app on the next enable.
    auto down_st = this->state();
    if (down_st != ClientState::IDLE && down_st != ClientState::INIT) {
      this->release_services();
      this->set_idle_();
      this->listener_->on_connection_state(false, 0, ble_device_base::GATT_ERR_NOT_CONNECTED);
    }
    this->set_state(ClientState::INIT);
    return;
  }
  auto st = this->state();
  if (st == ClientState::INIT) {
    // Parity with BLEClientBase: a failed registration marks the slot
    // failed and idles it without retry.
    auto ret = esp_ble_gattc_app_register(this->app_id);
    if (ret) {
      ESP_LOGE(TAG, "gattc app register failed: app_id=%d code=%d", this->app_id, ret);
      this->mark_failed();
    }
    // Do not wait for REG_EVT; a dropped event must not wedge the slot.
    this->set_idle_();
  } else if (st == ClientState::DISCONNECTING || this->disconnect_pending()) {
    // The one teardown safety net: a lost CLOSE_EVT, or a scheduled
    // teardown whose OPEN_EVT never arrives.
    if (millis() - this->disconnecting_started_ > ble_device_base::GATT_DISCONNECT_TIMEOUT_MS) {
      ESP_LOGE(TAG, "[%d] Timeout waiting for teardown, forcing IDLE", this->connection_index_);
      // Release before idling: a lost completion must not leak the cache.
      this->release_services();
      this->set_idle_();  // also clears want_disconnect_
      this->listener_->on_connection_state(false, 0, ESP_GATT_CONN_TIMEOUT);
    }
  } else {
    // The loop stays on while a link exists (stack-down watch, pre-started
    // search flush); it settles only back at IDLE.
    this->deliver_pending_search_();
    if (this->state() == ClientState::IDLE) {
      this->disable_loop();
    }
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
  // Only from idle: clobbering DISCONNECTING would open a new link the
  // stale CLOSE_EVT then tears down.
  if (this->state() != ClientState::IDLE) {
    ESP_LOGW(TAG, "[%d] Connect rejected, slot busy", this->connection_index_);
    return ESP_GATT_BUSY;
  }
  ble_device_base::uint64_to_mac_msb_first(address, this->remote_bda_);
  this->remote_addr_type_ = addr_type;
  // Hand the request to the tracker's promote loop: it stops the scan, raises
  // coex, and calls tracker_connect_() - the tracker owns connect timing here.
  this->set_state(ClientState::DISCOVERED);
  return 0;
}

void BluedroidGattClient::tracker_connect_() {
  auto st = this->state();
  if (st == ClientState::CONNECTING || st == ClientState::CONNECTED || st == ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%d] Connection already in progress", this->connection_index_);
    return;
  }
  if (st == ClientState::DISCONNECTING) {
    ESP_LOGW(TAG, "[%d] Cannot connect, still waiting for CLOSE_EVT", this->connection_index_);
    return;
  }
  ESP_LOGI(TAG, "[%d] 0x%02x Connecting", this->connection_index_, this->remote_addr_type_);
  // Per-attempt latches; the search machine is reset by set_idle_(), the
  // one door back to IDLE.
  this->services_released_ = false;
  this->seen_mtu_ = false;
  this->mtu_failed_ = false;
  this->enable_loop();
  this->set_state(ClientState::CONNECTING);
  if (this->connection_type_ == ConnectionType::V3_WITHOUT_CACHE) {
    // Fast params for the discovery phase; stepped down at SEARCH_CMPL.
    this->check_and_log_error_("esp_ble_gap_set_prefer_conn_params",
                               esp_ble_gap_set_prefer_conn_params(this->remote_bda_, FAST_MIN_CONN_INTERVAL,
                                                                  FAST_MAX_CONN_INTERVAL, 0, FAST_CONN_TIMEOUT));
  } else {
    this->check_and_log_error_("esp_ble_gap_set_prefer_conn_params",
                               esp_ble_gap_set_prefer_conn_params(this->remote_bda_, MEDIUM_MIN_CONN_INTERVAL,
                                                                  MEDIUM_MAX_CONN_INTERVAL, 0, MEDIUM_CONN_TIMEOUT));
  }
  auto ret = esp_ble_gattc_open(this->gattc_if_, this->remote_bda_,
                                static_cast<esp_ble_addr_type_t>(this->remote_addr_type_), true);
  if (ret) {
    this->log_gattc_warning_("esp_ble_gattc_open", ret);
    // CONNECT_EVT never fired; nothing to close.
    this->set_idle_();
    this->listener_->on_connection_state(false, 0, ret);
  }
}

int BluedroidGattClient::gatt_disconnect() {
  auto st = this->state();
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
    this->set_idle_();
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  if (st == ClientState::CONNECTING || this->conn_id_ == UNSET_CONN_ID) {
    ESP_LOGD(TAG, "[%d] Disconnect scheduled", this->connection_index_);
    this->want_disconnect_ = true;
    // Arm the safety window: a lost OPEN_EVT must not leak the teardown.
    this->disconnecting_started_ = millis();
    this->enable_loop();
    return 0;
  }
  this->unconditional_disconnect_();
  return 0;
}

void BluedroidGattClient::unconditional_disconnect_() {
  ESP_LOGI(TAG, "[%d] Disconnecting (conn_id: %d)", this->connection_index_, this->conn_id_);
  if (this->conn_id_ == UNSET_CONN_ID) {
    // Terminal state now rather than leaning on the scheduled-teardown timer.
    ESP_LOGE(TAG, "[%d] conn id unset, cannot disconnect", this->connection_index_);
    this->release_services();
    this->set_idle_();
    this->listener_->on_connection_state(false, 0, ble_device_base::GATT_ERR_NOT_CONNECTED);
    return;
  }
  auto err = esp_ble_gattc_close(this->gattc_if_, this->conn_id_);
  if (err != ESP_OK) {
    // The stack is now in an indeterminate state for this link.
    ESP_LOGE(TAG, "[%d] esp_ble_gattc_close error: %d", this->connection_index_, err);
  }
  this->set_disconnecting_();
}

bool BluedroidGattClient::cancel_gatt_disconnect() {
  // Only a scheduled teardown (want_disconnect_ latched while the open is
  // still in flight) is cancellable; once closing started the terminal
  // report settles the race.
  if (this->state() != ClientState::CONNECTING || !this->disconnect_pending()) {
    return false;
  }
  this->want_disconnect_ = false;
  return true;
}

int BluedroidGattClient::discover_services() {
  if (this->conn_id_ == UNSET_CONN_ID) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  switch (this->search_state_) {
    case SearchState::PRESTARTED:
      // The pending SEARCH_CMPL reports once it lands.
      this->search_state_ = SearchState::CLAIMED;
      return 0;
    case SearchState::PRESTART_DONE:
      // Already landed: the flush after the connected report delivers
      // (loop() covers a claim made outside that event drain).
      this->search_state_ = SearchState::REPORT_PENDING;
      this->enable_loop();
      return 0;
    case SearchState::CLAIMED:
    case SearchState::REPORT_PENDING:
      return 0;  // One completion is already owed to this claimant.
    case SearchState::NONE:
      break;
  }
  int err = this->check_and_log_error_("esp_ble_gattc_search_service",
                                       esp_ble_gattc_search_service(this->gattc_if_, this->conn_id_, nullptr));
  if (err == 0) {
    this->search_state_ = SearchState::CLAIMED;
  }
  return err;
}

int BluedroidGattClient::read_characteristic(uint16_t handle) {
  if (this->conn_id_ == UNSET_CONN_ID) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  return this->check_and_log_error_("esp_ble_gattc_read_char", esp_ble_gattc_read_char(this->gattc_if_, this->conn_id_,
                                                                                       handle, ESP_GATT_AUTH_REQ_NONE));
}

int BluedroidGattClient::write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) {
  if (this->conn_id_ == UNSET_CONN_ID) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  // The BTC layer copies the payload immediately, so the const_cast is safe.
  return this->check_and_log_error_(
      "esp_ble_gattc_write_char",
      esp_ble_gattc_write_char(this->gattc_if_, this->conn_id_, handle, len, const_cast<uint8_t *>(data),
                               response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP,
                               ESP_GATT_AUTH_REQ_NONE));
}

int BluedroidGattClient::read_descriptor(uint16_t handle) {
  if (this->conn_id_ == UNSET_CONN_ID) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  return this->check_and_log_error_(
      "esp_ble_gattc_read_char_descr",
      esp_ble_gattc_read_char_descr(this->gattc_if_, this->conn_id_, handle, ESP_GATT_AUTH_REQ_NONE));
}

int BluedroidGattClient::write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) {
  if (this->conn_id_ == UNSET_CONN_ID) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  return this->check_and_log_error_(
      "esp_ble_gattc_write_char_descr",
      esp_ble_gattc_write_char_descr(this->gattc_if_, this->conn_id_, handle, len, const_cast<uint8_t *>(data),
                                     ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE));
}

int BluedroidGattClient::notify_characteristic(uint16_t handle, bool enable) {
  if (this->conn_id_ == UNSET_CONN_ID) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  // Local registration only; the CCCD write is the API client's responsibility.
  if (enable) {
    return this->check_and_log_error_("esp_ble_gattc_register_for_notify",
                                      esp_ble_gattc_register_for_notify(this->gattc_if_, this->remote_bda_, handle));
  }
  return this->check_and_log_error_("esp_ble_gattc_unregister_for_notify",
                                    esp_ble_gattc_unregister_for_notify(this->gattc_if_, this->remote_bda_, handle));
}

int BluedroidGattClient::pair() {
  if (this->conn_id_ == UNSET_CONN_ID) {
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  }
  return esp_ble_set_encryption(this->remote_bda_, ESP_BLE_SEC_ENCRYPT);
}

int BluedroidGattClient::update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                                  uint16_t timeout) {
  return this->update_conn_params_(min_interval, max_interval, latency, timeout, "custom");
}

void BluedroidGattClient::release_services() {
  this->service_total_ = 0;
  // Always set: terminates any in-flight stream on every cache config.
  this->services_released_ = true;
#ifndef CONFIG_BT_GATTC_CACHE_NVS_FLASH
  // A failed clean leaves a stale database the next connection could serve
  // as authoritative. A disabled stack invalidates its own cache; skip the
  // meaningless call instead of warning on every OTA/ble.disable teardown.
  if (esp32_ble::global_ble->is_active()) {
    this->check_and_log_error_("esp_ble_gattc_cache_clean", esp_ble_gattc_cache_clean(this->remote_bda_));
  }
#endif
}

// ---- internals ----

bool BluedroidGattClient::check_addr_(const esp_bd_addr_t &addr) const {
  return memcmp(addr, this->remote_bda_, sizeof(esp_bd_addr_t)) == 0;
}

void BluedroidGattClient::set_idle_() {
  this->set_state(ClientState::IDLE);
  this->conn_id_ = UNSET_CONN_ID;
  this->search_state_ = SearchState::NONE;
  this->search_status_ = 0;
}

void BluedroidGattClient::set_disconnecting_() {
  this->disconnecting_started_ = millis();
  this->set_state(ClientState::DISCONNECTING);
  // The loop may be disabled while idle; the safety timeout needs it.
  this->enable_loop();
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

int BluedroidGattClient::handle_search_cmpl_(esp_gatt_status_t status) {
  // Step down from the fast discovery params.
  this->update_conn_params_(MEDIUM_MIN_CONN_INTERVAL, MEDIUM_MAX_CONN_INTERVAL, 0, MEDIUM_CONN_TIMEOUT, "medium");
  if (status != ESP_GATT_OK) {
    // A failed discovery reads as a clean zero from the count calls below;
    // honoring the event status stops it becoming an authoritative empty
    // list.
    return status;
  }
  uint16_t primary = 0;
  uint16_t secondary = 0;
  auto primary_status = esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_PRIMARY_SERVICE,
                                                     0x0001, 0xFFFF, 0, &primary);
  auto secondary_status = esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_SECONDARY_SERVICE,
                                                       0x0001, 0xFFFF, 0, &secondary);
  if (primary_status != ESP_GATT_OK || secondary_status != ESP_GATT_OK) {
    // A failed count must not become an authoritative empty database.
    auto count_status = primary_status != ESP_GATT_OK ? primary_status : secondary_status;
    this->log_gattc_warning_("esp_ble_gattc_get_attr_count", count_status);
    return count_status;
  }
  this->service_total_ = primary + secondary;
  return 0;
}

// Reports a completed search once claimed; delivery consumes the state so
// a re-discovery issues a real search.
void BluedroidGattClient::deliver_pending_search_() {
  if (this->search_state_ != SearchState::REPORT_PENDING)
    return;
  this->search_state_ = SearchState::NONE;
  this->listener_->on_service_discovery_done(this->search_status_);
}

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
// The wrapper's compile-time streamer detection must keep finding this
// method; a signature drift would silently fall back to the table streamer,
// which proxy builds compile without a materializer.
static_assert(requires(BluedroidGattClient c, BluetoothConnection &conn) { c.stream_service_batch(conn); });

// Bound by the SERVICE STREAMING HAZARD note at the top of
// bluetooth_connection_hub.cpp: never skip a batch, never send done early.
void BluedroidGattClient::stream_service_batch(BluetoothConnection &conn) {
  if (this->services_released_) {
    // Released under the stream: park without services-done so a partial
    // list is never cached as authoritative (the client retries after its
    // GetServices timeout).
    ESP_LOGW(TAG, "[%d] [%s] Services released mid-stream, parking", conn.connection_index_, conn.address_str_);
    conn.send_service_ = DONE_SENDING_SERVICES;
    return;
  }
  if (conn.send_service_ >= this->service_total_) {
    this->release_services();
    conn.send_services_done_();
    return;
  }

  // The subscriber vanished mid-stream.
  auto *api_conn = conn.proxy_->get_api_connection();
  if (api_conn == nullptr) {
    ESP_LOGW(TAG, "[%d] [%s] API connection lost while streaming services", conn.connection_index_, conn.address_str_);
    conn.park_service_stream_();
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
    esp_gatt_status_t svc_status = esp_ble_gattc_get_service(this->gattc_if_, this->conn_id_, nullptr, &service_result,
                                                             &svc_count, conn.send_service_);
    if (svc_status != ESP_GATT_OK || svc_count == 0) {
      ESP_LOGE(TAG, "[%d] [%s] Service walk failed (service %d), aborting stream", conn.connection_index_,
               conn.address_str_, conn.send_service_);
      conn.abort_service_stream(svc_status != ESP_GATT_OK ? svc_status : ESP_GATT_NOT_FOUND);
      return;
    }
    uint16_t total_char_count = 0;
    auto char_count_status =
        esp_ble_gattc_get_attr_count(this->gattc_if_, this->conn_id_, ESP_GATT_DB_CHARACTERISTIC,
                                     service_result.start_handle, service_result.end_handle, 0, &total_char_count);
    if (char_count_status != ESP_GATT_OK) {
      this->log_gattc_warning_("esp_ble_gattc_get_attr_count", char_count_status);
      conn.abort_service_stream(char_count_status);
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
        if (char_status != ESP_GATT_OK || cc == 0) {
          // An early terminator contradicts the count from the same cache;
          // never stream a silently truncated list.
          this->log_gattc_warning_("esp_ble_gattc_get_all_char", char_status);
          conn.abort_service_stream(char_status != ESP_GATT_OK ? char_status : ESP_GATT_NOT_FOUND);
          return;
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
          conn.abort_service_stream(desc_count_status);
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
            if (desc_status != ESP_GATT_OK || dc == 0) {
              this->log_gattc_warning_("esp_ble_gattc_get_all_descr", desc_status);
              conn.abort_service_stream(desc_status != ESP_GATT_OK ? desc_status : ESP_GATT_NOT_FOUND);
              return;
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
    conn.note_batch_stalled_();
    conn.send_service_ = batch_start;
    return;
  }
  conn.batch_stalled_ = false;
}
#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS

// ---- events ----

void BluedroidGattClient::handle_open_evt_(esp_ble_gattc_cb_param_t *param) {
  auto st = this->state();
  if (st == ClientState::IDLE) {
    // Late OPEN_EVT after the slot went IDLE (open-error race, or the
    // teardown net gave up): close a won link, never resurrect the slot.
    ESP_LOGD(TAG, "[%d] OPEN_EVT in IDLE state (status=%d)", this->connection_index_, param->open.status);
    if (param->open.status == ESP_GATT_OK || param->open.status == ESP_GATT_ALREADY_OPEN) {
      // A failed close here leaks a live link nothing tracks; make it heard.
      this->check_and_log_error_("esp_ble_gattc_close", esp_ble_gattc_close(this->gattc_if_, param->open.conn_id));
    }
    return;
  }
  if (st != ClientState::CONNECTING) {
    ESP_LOGE(TAG, "[%d] OPEN_EVT in unexpected state", this->connection_index_);
  }
  if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
    this->log_gattc_warning_("Connection open", param->open.status);
    // Never established, CLOSE_EVT may not follow.
    this->set_idle_();
    this->listener_->on_connection_state(false, 0, param->open.status);
    return;
  }
  if (this->disconnect_pending()) {
    // Open resolved with a teardown scheduled: close now (conn_id_ stays set
    // so CLOSE_EVT still matches).
    this->unconditional_disconnect_();
    return;
  }
  this->set_state(ClientState::CONNECTED);
  ESP_LOGI(TAG, "[%d] Connection open", this->connection_index_);
  if (this->connection_type_ == ConnectionType::V3_WITH_CACHE) {
    this->set_state(ClientState::ESTABLISHED);
    // No discovery phase: report immediately with the default MTU. The
    // cached path never waits for (or reports) the exchange - seen_mtu_
    // suppresses the CFG_MTU report, matching the previous esp32 behavior.
    this->seen_mtu_ = true;
    this->listener_->on_connection_state(true, ble_device_base::DEFAULT_ATT_MTU, 0);
  } else {
    // Discovery-bound connection: start the search now so it overlaps the
    // MTU exchange. On a refusal fall back to the serialized path - the
    // consumer's own discover_services() call retries the real search.
    if (this->check_and_log_error_("esp_ble_gattc_search_service",
                                   esp_ble_gattc_search_service(this->gattc_if_, param->open.conn_id, nullptr)) == 0) {
      this->search_state_ = SearchState::PRESTARTED;
    }
    if (this->mtu_failed_ && !this->seen_mtu_) {
      // Refused MTU request: report with the default so the consumer
      // proceeds.
      this->seen_mtu_ = true;
      this->listener_->on_connection_state(true, ble_device_base::DEFAULT_ATT_MTU, 0);
      this->deliver_pending_search_();
    }
  }
}

void BluedroidGattClient::handle_disconnect_evt_(esp_ble_gattc_cb_param_t *param) {
  if (param->disconnect.reason == ESP_GATT_CONN_TERMINATE_PEER_USER && this->state() == ClientState::CONNECTED) {
    ESP_LOGW(TAG, "[%d] Remote closed during discovery", this->connection_index_);
  } else {
    ESP_LOGD(TAG, "[%d] DISCONNECT_EVT reason=0x%02x", this->connection_index_, param->disconnect.reason);
  }
  if (this->state() == ClientState::IDLE) {
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

bool BluedroidGattClient::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t esp_gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  if (event == ESP_GATTC_REG_EVT && this->app_id != param->reg.app_id)
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
        // No CFG_MTU_EVT will follow; OPEN_EVT reports with the default.
        this->mtu_failed_ = true;
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
      }
      if (!this->seen_mtu_ && !this->disconnect_pending() && this->state() != ClientState::DISCONNECTING) {
        // Teardown owns the link: suppress the connected report here like
        // OPEN_EVT and SEARCH_CMPL do; the terminal report settles it.
        this->seen_mtu_ = true;
        // The connected report waited for the MTU; forwarded, not stored.
        this->listener_->on_connection_state(
            true, param->cfg_mtu.status == ESP_GATT_OK ? param->cfg_mtu.mtu : ble_device_base::DEFAULT_ATT_MTU, 0);
        // The consumer requests discovery from inside that report; when the
        // pre-started search already finished, complete it in the same drain.
        this->deliver_pending_search_();
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
      this->listener_->on_connection_state(false, 0, param->close.reason);
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (this->conn_id_ != param->search_cmpl.conn_id)
        return false;
      ESP_LOGI(TAG, "[%d] Service discovery complete", this->connection_index_);
      if (this->state() == ClientState::DISCONNECTING) {
        // Teardown owns the link; the result is never delivered, skip the
        // work.
        break;
      }
      this->search_status_ = this->handle_search_cmpl_(static_cast<esp_gatt_status_t>(param->search_cmpl.status));
      this->search_state_ =
          this->search_state_ == SearchState::CLAIMED ? SearchState::REPORT_PENDING : SearchState::PRESTART_DONE;
      this->set_state(ClientState::ESTABLISHED);
      this->deliver_pending_search_();
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

void BluedroidGattClient::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_SEC_REQ_EVT: {
      if (!this->check_addr_(param->ble_security.auth_cmpl.bd_addr))
        break;
      // Always accept; a refused response means no AUTH_CMPL, so answer the
      // pairing request with the failure.
      int sec_err = this->check_and_log_error_("esp_ble_gap_security_rsp",
                                               esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true));
      if (sec_err != 0) {
        this->listener_->on_pairing_result(sec_err);
      }
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
