#include "ble_client_gatt.h"

#if defined(USE_BLE_GATT_CLIENT) && !defined(USE_ESP32)

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::ble_client {

static const char *const TAG = "ble_client";

// Hold-off step per consecutive failure; capped so a flapping peer retries
// within a minute at worst.
static const uint32_t FAILURE_HOLD_OFF_STEP_MS = 10000;
static const uint8_t FAILURE_HOLD_OFF_MAX_STEPS = 6;

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
  }
  // Enabling does not connect: the next sighting does (legacy parity).
}

bool BLEClient::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;
  // The sighting is the source of truth for the address type.
  this->address_type_ = device.get_address_type();
  if (!this->enabled || !this->auto_connect_ || this->state_ != State::IDLE)
    return true;
  if (this->hold_off_ms_ != 0 && millis() - this->hold_off_start_ < this->hold_off_ms_)
    return true;
  this->attempt_connect_();
  return true;
}

void BLEClient::connect() {
  if (this->state_ != State::IDLE)
    return;
  // An absent peer can inhibit scanning for the backend's full connect
  // timeout, so this is worth a breadcrumb - but it is a supported action.
  ESP_LOGI(TAG, "[%s] Connecting on request", this->address_str_);
  this->attempt_connect_();
}

void BLEClient::attempt_connect_() {
  int err = this->backend_->connect(this->address_, this->address_type_);
  if (err != 0) {
    // A refused connect never produces a callback: stay idle, charge the
    // backoff, and resolve any waiting connect action through the failure
    // path so its chain terminates.
    ESP_LOGW(TAG, "[%s] Connect refused, err=%d", this->address_str_, err);
    this->register_failure_();
    this->defer([this]() { this->connect_failed_callbacks_.call(); });
    return;
  }
  ESP_LOGD(TAG, "[%s] Connecting", this->address_str_);
  this->state_ = State::CONNECTING;
}

void BLEClient::disconnect() {
  if (this->state_ == State::IDLE)
    return;
  // A deliberate teardown's failure report must not feed the backoff.
  this->cancel_requested_ = true;
  if (this->backend_->gatt_disconnect() != 0) {
    // Refused synchronously: the backend is already down and no report will
    // come; settle through the normal path so waiters resolve.
    this->on_connection_state(false, 0, 0);
  }
}

void BLEClient::register_failure_() {
  if (this->consecutive_failures_ < FAILURE_HOLD_OFF_MAX_STEPS)
    this->consecutive_failures_++;
  this->hold_off_start_ = millis();
  this->hold_off_ms_ = this->consecutive_failures_ * FAILURE_HOLD_OFF_STEP_MS;
  ESP_LOGW(TAG, "[%s] Holding off reconnect for %u s", this->address_str_,
           this->consecutive_failures_ * (FAILURE_HOLD_OFF_STEP_MS / 1000));
}

void BLEClient::on_connection_state(bool connected, uint16_t mtu, int error) {
  if (connected) {
    this->state_ = State::DISCOVERING;
    if (this->backend_->discover_services() != 0) {
      // Synchronous refusal: no discovery completion will follow.
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
      ESP_LOGD(TAG, "[%s] Connect attempt cancelled", this->address_str_);
    } else {
      ESP_LOGW(TAG, "[%s] Connect failed, status=%d", this->address_str_, error);
      this->register_failure_();
    }
    this->defer([this]() { this->connect_failed_callbacks_.call(); });
  }
}

void BLEClient::on_service_discovery_done(int error) {
  if (error != 0) {
    ESP_LOGW(TAG, "[%s] Service discovery failed, status=%d", this->address_str_, error);
    this->register_failure_();
    // The teardown is deliberate: do not charge the backoff again for its
    // connection report.
    this->disconnect();
    return;
  }
  auto table = this->backend_->get_service_table();
  for (auto *node : this->nodes_) {
    node->on_connected(table);
  }
  this->backend_->release_services();
  this->state_ = State::CONNECTED;
  this->consecutive_failures_ = 0;
  this->hold_off_ms_ = 0;
  ESP_LOGI(TAG, "[%s] Connected", this->address_str_);
  this->defer([this]() { this->connect_callbacks_.call(); });
}

void BLEClient::on_write_result(uint16_t handle, int error) {
  for (auto *node : this->nodes_) {
    node->on_write_result(handle, error);
  }
}

void BLEClient::on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) {
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

#endif  // USE_BLE_GATT_CLIENT && !USE_ESP32
