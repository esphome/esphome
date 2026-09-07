#include "ble_client_gatt.h"

#if defined(USE_BLE_GATT_CLIENT) && !defined(USE_BLE_CLIENT_LEGACY_ENGINE)

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::ble_client {

static const char *const TAG = "ble_client";

void BLEClient::register_ble_node(BLEClientNode *node) {
  node->set_ble_client_parent(this);
  if (this->nodes_.size() == ESPHOME_BLE_CLIENT_MAX_NODES) {
    // push_back past capacity is a silent no-op; an undersized slot count
    // must be loud at boot, not an unresolvable node at runtime.
    ESP_LOGE(TAG, "[%s] Node capacity exceeded; node dropped", this->address_str_);
    this->status_set_error(LOG_STR("node capacity exceeded"));
    return;
  }
  this->nodes_.push_back(node);
}

void BLEClient::set_address(uint64_t address) {
  this->address_ = address;
  uint8_t mac[6];
  ble_device_base::uint64_to_mac_msb_first(address, mac);
  format_mac_addr_upper(mac, this->address_str_);
}

void BLEClient::set_enabled(bool enabled) {
  if (enabled == this->enabled)
    return;
  ESP_LOGI(TAG, "[%s] %s", this->address_str_, enabled ? "Enabled" : "Disabled");
  this->enabled = enabled;
  if (!enabled) {
    this->disconnect();
    return;
  }
  // A re-enable clears the backoff; the next sighting connects (legacy
  // parity: enabling does not itself connect).
  this->backoff_.reset();
}

bool BLEClient::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;
  // The sighting is the source of truth for the address type.
  this->address_type_ = device.get_address_type();
  this->address_type_known_ = true;
  if (!this->enabled || !this->auto_connect_ || this->state_ != State::IDLE)
    return true;
  if (this->backoff_.holding_off())
    return true;
  this->attempt_connect_();
  return true;
}

void BLEClient::connect() {
  if (this->state_ != State::IDLE) {
    ESP_LOGD(TAG, "[%s] Connect requested while busy, ignoring", this->address_str_);
    return;
  }
  // An absent peer can inhibit scanning for the backend's full connect
  // timeout, so this is worth a breadcrumb - but it is a supported action.
  ESP_LOGI(TAG, "[%s] Connecting on request", this->address_str_);
  if (!this->address_type_known_) {
    // Legacy parity: without a sighting the address type defaults to
    // public, which never matches a random-static peer.
    ESP_LOGW(TAG, "[%s] No sighting yet; assuming a public address type", this->address_str_);
  }
  this->attempt_connect_();
}

void BLEClient::attempt_connect_() {
  int err = this->backend_->connect(this->address_, this->address_type_);
  if (err != 0) {
    // A refused connect never produces a callback: stay idle, charge the
    // backoff, and resolve any waiting connect action through the failure
    // path so its chain terminates.
    ESP_LOGW(TAG, "[%s] Connect refused, err=%d", this->address_str_, err);
    this->backoff_.register_failure(this->address_str_);
    this->defer([this]() { this->connect_failed_callbacks_.call(); });
    return;
  }
  ESP_LOGD(TAG, "[%s] Connecting", this->address_str_);
  this->state_ = State::CONNECTING;
}

void BLEClient::disconnect() {
  if (this->state_ == State::IDLE) {
    ESP_LOGD(TAG, "[%s] Disconnect requested while idle, ignoring", this->address_str_);
    return;
  }
  // A deliberate teardown's failure report must not feed the backoff.
  this->cancel_requested_ = true;
  int err = this->backend_->gatt_disconnect();
  if (err != 0) {
    // Refused synchronously: backend and client disagree about the link
    // state. Warn, then settle through the deliberate-cancel path.
    ESP_LOGW(TAG, "[%s] Disconnect refused, err=%d; settling locally", this->address_str_, err);
    this->on_connection_state(false, 0, err);
  }
}

void BLEClient::on_connection_state(bool connected, uint16_t mtu, int error) {
  if (connected) {
    this->state_ = State::DISCOVERING;
    int discover_err = this->backend_->discover_services();
    if (discover_err != 0) {
      // Synchronous refusal: no discovery completion will follow.
      ESP_LOGW(TAG, "[%s] Service discovery refused, err=%d", this->address_str_, discover_err);
      this->backoff_.register_failure(this->address_str_);
      // Deliberate teardown: its report must not charge the backoff again.
      this->disconnect();
    }
    return;
  }
  bool was_connected = this->state_ == State::CONNECTED;
  bool cancelled = this->cancel_requested_;
  this->cancel_requested_ = false;
  this->state_ = State::IDLE;
  if (was_connected) {
    ESP_LOGI(TAG, "[%s] Disconnected, status=%d", this->address_str_, error);
    for (auto *node : this->nodes_) {
      node->on_disconnected();
    }
    // Continuations leave the backend's event-drain stack first.
    this->defer([this]() { this->disconnect_callbacks_.call(); });
  } else {
    if (cancelled) {
      // status carries the refusal code when the teardown settled
      // synchronously; 0 on a backend-completed cancel.
      ESP_LOGD(TAG, "[%s] Connect attempt cancelled, status=%d", this->address_str_, error);
    } else {
      ESP_LOGW(TAG, "[%s] Connect failed, status=%d", this->address_str_, error);
      this->backoff_.register_failure(this->address_str_);
    }
    this->defer([this]() { this->connect_failed_callbacks_.call(); });
  }
}

void BLEClient::on_service_discovery_done(int error) {
  if (error != 0) {
    ESP_LOGW(TAG, "[%s] Service discovery failed, status=%d", this->address_str_, error);
    this->backoff_.register_failure(this->address_str_);
    // The teardown is deliberate: do not charge the backoff again for its
    // connection report.
    this->disconnect();
    return;
  }
  ble_device_base::GattServiceTable table{};
  if (!this->nodes_.empty()) {
    // Materialize only when a node will read it: a client with no nodes
    // would pay the build/free cycle on every (re)connect for nothing.
    table = this->backend_->get_service_table();
    if (table.service_count == 0) {
      // A failed materialization is indistinguishable from a service-less
      // peer, and a real GATT peer always exposes at least GAP/GATT: fail
      // the discovery before CONNECTED so the teardown resolves through
      // connect_failed, never a spurious on_disconnect.
      ESP_LOGW(TAG, "[%s] Service table is empty; treating as failed discovery", this->address_str_);
      this->backend_->release_services();
      this->backoff_.register_failure(this->address_str_);
      this->disconnect();
      return;
    }
  }
  // CONNECTED before the fan-out so nodes may consult connected() from
  // their own on_connected().
  this->state_ = State::CONNECTED;
  for (auto *node : this->nodes_) {
    node->on_connected(table);
    if (this->state_ != State::CONNECTED || this->cancel_requested_) {
      // A node tore the link down mid-fan-out: on_disconnect fires with no
      // preceding on_connect, so leave a trace of why.
      ESP_LOGW(TAG, "[%s] A node aborted the connection during setup", this->address_str_);
      this->backend_->release_services();
      return;
    }
  }
  this->backend_->release_services();
  this->backoff_.reset();
  ESP_LOGI(TAG, "[%s] Connected", this->address_str_);
  this->defer([this]() { this->connect_callbacks_.call(); });
}

void BLEClient::on_write_result(uint16_t handle, int error) {
  if (error != 0) {
    // Breadcrumb even when no node claims the handle.
    ESP_LOGD(TAG, "[%s] Write on handle 0x%04x completed with status %d", this->address_str_, handle, error);
  }
  for (auto *node : this->nodes_) {
    node->on_write_result(handle, error);
  }
}

void BLEClient::on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) {
  if (error != 0) {
    // Breadcrumb even when no node claims the handle.
    ESP_LOGD(TAG, "[%s] Read on handle 0x%04x completed with status %d", this->address_str_, handle, error);
  }
  for (auto *node : this->nodes_) {
    node->on_read_result(handle, data, len, error);
  }
}

void BLEClient::on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) {
  // Every node sees every notification and filters by handle (legacy parity).
  for (auto *node : this->nodes_) {
    node->on_notify(handle, data, len);
  }
}

void BLEClient::on_notify_state(uint16_t handle, bool enabled, int error) {
  if (error != 0) {
    ESP_LOGW(TAG, "[%s] Notify %s on handle 0x%04x failed, status=%d", this->address_str_,
             enabled ? "enable" : "disable", handle, error);
  }
  for (auto *node : this->nodes_) {
    node->on_notify_state(handle, enabled, error);
  }
}

void BLEClient::on_pairing_result(int status) {
  if (status != 0) {
    ESP_LOGW(TAG, "[%s] Pairing failed, status=%d", this->address_str_, status);
  } else {
    ESP_LOGI(TAG, "[%s] Paired", this->address_str_);
  }
  for (auto *node : this->nodes_) {
    node->on_pairing_result(status);
  }
}

void BLEClient::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BLE Client:\n"
                "  Address: %s\n"
                "  Auto connect: %s",
                this->address_str_, YESNO(this->auto_connect_));
  if (this->enabled && this->state_ == State::IDLE) {
    ESP_LOGCONFIG(TAG, "  Waiting for an advertisement from the device");
  }
}

}  // namespace esphome::ble_client

#endif  // USE_BLE_GATT_CLIENT && !USE_BLE_CLIENT_LEGACY_ENGINE
