#include "ble_client.h"
#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#ifdef USE_BLE_CLIENT_GATT_NODES
#include "esphome/components/bluetooth_connection/bluetooth_connection.h"
#include "esphome/components/bluetooth_connection/gatt_service_table_bluedroid.h"
#endif

namespace esphome::ble_client {

static const char *const TAG = "ble_client";

#ifdef USE_BLE_CLIENT_GATT_NODES
// Hold-off per consecutive materializer failure (neutral-engine parity).
static const uint32_t GATT_FAILURE_HOLD_OFF_STEP_MS = 10000;
static const uint8_t GATT_FAILURE_HOLD_OFF_MAX_STEPS = 6;
#endif

void BLEClient::setup() {
  BLEClientBase::setup();
  this->enabled = true;
}

void BLEClient::loop() {
  BLEClientBase::loop();
  for (auto *node : this->nodes_)
    node->loop();
}

void BLEClient::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Client:");
  BLEClientBase::dump_config();
}

bool BLEClient::parse_device(const espbt::ESPBTDevice &device) {
  if (!this->enabled)
    return false;
#ifdef USE_BLE_CLIENT_GATT_NODES
  if (this->gatt_hold_off_ms_ != 0 && millis() - this->gatt_hold_off_start_ < this->gatt_hold_off_ms_ &&
      device.address_uint64() == this->address_)
    return false;
#endif
  return BLEClientBase::parse_device(device);
}

void BLEClient::set_enabled(bool enabled) {
  if (enabled == this->enabled)
    return;
  this->enabled = enabled;
  if (!enabled) {
    ESP_LOGI(TAG, "[%s] Disabling BLE client.", this->address_str());
    this->disconnect();
    return;
  }
#ifdef USE_BLE_CLIENT_GATT_NODES
  // A re-enable clears the backoff (neutral-engine parity).
  this->gatt_consecutive_failures_ = 0;
  this->gatt_hold_off_ms_ = 0;
#endif
}

bool BLEClient::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t esp_gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
#ifdef USE_BLE_CLIENT_GATT_NODES
  // Bridge-initiated registrations bypass the base's REG_FOR_NOTIFY handling:
  // its automatic CCCD write would double the node's own.
  // Handle-keyed: mixed legacy/neutral subscriptions to one characteristic
  // are unsupported during the migration window.
  if (event == ESP_GATTC_REG_FOR_NOTIFY_EVT && esp_gattc_if == this->gattc_if_ &&
      this->take_pending_gatt_reg_(param->reg_for_notify.handle)) {
    if (this->pending_notify_regs_ > 0)
      this->pending_notify_regs_--;
    int err = param->reg_for_notify.status == ESP_GATT_OK ? 0 : param->reg_for_notify.status;
    for (auto *node : this->gatt_nodes_)
      node->on_notify_state(param->reg_for_notify.handle, true, err);
    // A retiring last registration must still release the cache.
    this->maybe_release_services_();
    return true;
  }
#endif
  if (!BLEClientBase::gattc_event_handler(event, esp_gattc_if, param))
    return false;

#ifdef USE_BLE_CLIENT_GATT_NODES
  // Before the legacy fan-out so gatt nodes resolve before any trigger fires.
  if (!this->dispatch_gatt_event_(event, param)) {
    // Failed discovery: the on_connect trigger must not fire into the teardown.
    return true;
  }
#endif
  for (auto *node : this->nodes_)
    node->gattc_event_handler(event, esp_gattc_if, param);
  this->maybe_release_services_();
  return true;
}

void BLEClient::maybe_release_services_() {
  // The release frees the GATT cache that BLEClientBase's CCCD lookup still needs.
  if (!this->services_.empty() && !this->notify_registration_pending() && this->all_nodes_established_()) {
    this->release_services();
    ESP_LOGD(TAG, "All clients established, services released");
  }
}

void BLEClient::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  BLEClientBase::gap_event_handler(event, param);

  for (auto *node : this->nodes_)
    node->gap_event_handler(event, param);
#ifdef USE_BLE_CLIENT_GATT_NODES
  if (event == ESP_GAP_BLE_AUTH_CMPL_EVT && this->check_addr(param->ble_security.auth_cmpl.bd_addr)) {
    int status = param->ble_security.auth_cmpl.success ? 0 : param->ble_security.auth_cmpl.fail_reason;
    for (auto *node : this->gatt_nodes_)
      node->on_pairing_result(status);
  }
#endif
}

void BLEClient::set_state(espbt::ClientState state) {
  BLEClientBase::set_state(state);
  // ESTABLISHED never flows through here; gatt nodes are promoted after the
  // on_connected fan-out.
  for (auto &node : nodes_)
    node->node_state = state;
}

bool BLEClient::all_nodes_established_() {
  if (this->state() != espbt::ClientState::ESTABLISHED)
    return false;
  for (auto &node : nodes_) {
    if (node->node_state != espbt::ClientState::ESTABLISHED)
      return false;
  }
  return true;
}

#ifdef USE_BLE_CLIENT_GATT_NODES

void BLEClient::register_gatt_node(BLEClientNode *node) {
  if (this->gatt_nodes_.size() == ESPHOME_BLE_CLIENT_MAX_NODES) {
    // push_back past capacity is a silent no-op; an undersized slot count
    // must be loud at boot, not an unresolvable node at runtime.
    ESP_LOGE(TAG, "[%s] Node capacity exceeded; node dropped", this->address_str());
    return;
  }
  this->gatt_nodes_.push_back(node);
  // nodes_ covers the shared state bookkeeping; gatt_nodes_ is the neutral
  // fan-out subset.
  this->register_ble_node(node);
}

bool BLEClient::take_pending_gatt_reg_(uint16_t handle) {
  // No duplicates (notify_characteristic refuses a re-push); swap-with-last.
  for (uint8_t i = 0; i < this->pending_gatt_reg_count_; i++) {
    if (this->pending_gatt_regs_[i] == handle) {
      this->pending_gatt_regs_[i] = this->pending_gatt_regs_[--this->pending_gatt_reg_count_];
      return true;
    }
  }
  return false;
}

bool BLEClient::dispatch_gatt_event_(esp_gattc_cb_event_t event, esp_ble_gattc_cb_param_t *param) {
  if (this->gatt_nodes_.empty())
    return true;
  switch (event) {
    case ESP_GATTC_SEARCH_CMPL_EVT:
      return this->handle_gatt_search_cmpl_(param->search_cmpl.status);
    case ESP_GATTC_READ_CHAR_EVT:
    case ESP_GATTC_READ_DESCR_EVT: {
      bool ok = param->read.status == ESP_GATT_OK;
      for (auto *node : this->gatt_nodes_) {
        node->on_read_result(param->read.handle, ok ? param->read.value : nullptr, ok ? param->read.value_len : 0,
                             ok ? 0 : param->read.status);
      }
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
    case ESP_GATTC_WRITE_DESCR_EVT:
      for (auto *node : this->gatt_nodes_) {
        node->on_write_result(param->write.handle, param->write.status == ESP_GATT_OK ? 0 : param->write.status);
      }
      break;
    case ESP_GATTC_NOTIFY_EVT:
      for (auto *node : this->gatt_nodes_) {
        node->on_notify(param->notify.handle, param->notify.value, param->notify.value_len);
      }
      break;
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT:
      // The base does no CCCD work for unregister; no interception needed.
      for (auto *node : this->gatt_nodes_) {
        node->on_notify_state(param->unreg_for_notify.handle, false,
                              param->unreg_for_notify.status == ESP_GATT_OK ? 0 : param->unreg_for_notify.status);
      }
      break;
    default:
      break;
  }
  return true;
}

// False = failed discovery: the link comes down and the caller suppresses
// the legacy fan-out.
bool BLEClient::handle_gatt_search_cmpl_(esp_gatt_status_t status) {
  // The base ignores the search status; the neutral contract must not.
  uint16_t service_total = 0;
  bool counted = status == ESP_GATT_OK && bluetooth_connection::BluedroidServiceTable::count_services(
                                              this->gattc_if_, this->conn_id_, &service_total);
  // Stack-owned; nodes copy their handles during on_connected().
  bluetooth_connection::BluedroidServiceTable table;
  if (!counted || service_total == 0 ||
      !table.build(this->gattc_if_, this->conn_id_, service_total, this->connection_index_)) {
    // Distinguishes a failed search/count from a genuinely service-less peer.
    ESP_LOGW(TAG, "[%s] Service table unavailable (status=%d, services=%u); treating as failed discovery",
             this->address_str(), status, service_total);
    this->register_gatt_failure_();
    this->disconnect();
    return false;
  }
  this->gatt_connected_ = true;
  auto view = table.view();
  for (auto *node : this->gatt_nodes_) {
    node->on_connected(view);
    if (this->state() != espbt::ClientState::ESTABLISHED) {
      // The node tore the link down; remaining nodes get on_disconnected
      // with no preceding on_connected, so leave a trace of why.
      ESP_LOGW(TAG, "[%s] A node aborted the connection during setup", this->address_str());
      return false;
    }
  }
  // Promote so the legacy release condition can fire.
  for (auto *node : this->gatt_nodes_)
    node->node_state = espbt::ClientState::ESTABLISHED;
  this->gatt_consecutive_failures_ = 0;
  this->gatt_hold_off_ms_ = 0;
  return true;
}

void BLEClient::register_gatt_failure_() {
  if (this->gatt_consecutive_failures_ < GATT_FAILURE_HOLD_OFF_MAX_STEPS)
    this->gatt_consecutive_failures_++;
  this->gatt_hold_off_start_ = millis();
  this->gatt_hold_off_ms_ = this->gatt_consecutive_failures_ * GATT_FAILURE_HOLD_OFF_STEP_MS;
  ESP_LOGW(TAG, "[%s] Holding off reconnect for %u s", this->address_str(),
           this->gatt_consecutive_failures_ * (GATT_FAILURE_HOLD_OFF_STEP_MS / 1000));
}

void BLEClient::on_disconnect_complete(esp_err_t reason) {
  this->pending_gatt_reg_count_ = 0;
  if (!this->gatt_connected_)
    return;  // Never-established links report nothing (neutral parity).
  this->gatt_connected_ = false;
  for (auto *node : this->gatt_nodes_)
    node->on_disconnected();
}

int BLEClient::check_gatt_op_(const char *operation, esp_err_t err) {
  if (err != ESP_OK)
    this->log_gattc_warning_(operation, err);
  return err;
}

int BLEClient::write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) {
  if (this->conn_id_ == UNSET_CONN_ID)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  return this->check_gatt_op_(
      "esp_ble_gattc_write_char",
      esp_ble_gattc_write_char(this->gattc_if_, this->conn_id_, handle, len, const_cast<uint8_t *>(data),
                               response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP,
                               ESP_GATT_AUTH_REQ_NONE));
}

int BLEClient::read_characteristic(uint16_t handle) {
  if (this->conn_id_ == UNSET_CONN_ID)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  return this->check_gatt_op_("esp_ble_gattc_read_char",
                              esp_ble_gattc_read_char(this->gattc_if_, this->conn_id_, handle, ESP_GATT_AUTH_REQ_NONE));
}

int BLEClient::read_descriptor(uint16_t handle) {
  if (this->conn_id_ == UNSET_CONN_ID)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  return this->check_gatt_op_(
      "esp_ble_gattc_read_char_descr",
      esp_ble_gattc_read_char_descr(this->gattc_if_, this->conn_id_, handle, ESP_GATT_AUTH_REQ_NONE));
}

int BLEClient::write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) {
  if (this->conn_id_ == UNSET_CONN_ID)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  return this->check_gatt_op_(
      "esp_ble_gattc_write_char_descr",
      esp_ble_gattc_write_char_descr(this->gattc_if_, this->conn_id_, handle, len, const_cast<uint8_t *>(data),
                                     ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE));
}

int BLEClient::notify_characteristic(uint16_t handle, bool enable) {
  if (this->conn_id_ == UNSET_CONN_ID)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  if (enable) {
    for (uint8_t i = 0; i < this->pending_gatt_reg_count_; i++) {
      if (this->pending_gatt_regs_[i] == handle) {
        // ESP_OK: the in-flight registration's completion fans out to all nodes.
        ESP_LOGW(TAG, "[%s] Notify registration already pending for handle 0x%04x", this->address_str(), handle);
        return ESP_OK;
      }
    }
    if (this->pending_gatt_reg_count_ == ESPHOME_BLE_CLIENT_MAX_NODES) {
      // An untracked registration would let the base's auto-CCCD through.
      ESP_LOGE(TAG, "[%s] Too many pending notify registrations", this->address_str());
      return ble_device_base::GATT_ERR_NO_MEMORY;
    }
    // The base helper's pending count holds the service-release until the
    // (intercepted) completion.
    esp_err_t err = this->register_for_notify(handle);
    if (err == ESP_OK)
      this->pending_gatt_regs_[this->pending_gatt_reg_count_++] = handle;
    return err;
  }
  return esp_ble_gattc_unregister_for_notify(this->gattc_if_, this->remote_bda_, handle);
}

int BLEClient::unpair() { return bluetooth_connection::unpair_device(this->get_address()); }

#endif  // USE_BLE_CLIENT_GATT_NODES

}  // namespace esphome::ble_client

#endif
