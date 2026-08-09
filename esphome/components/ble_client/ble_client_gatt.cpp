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
  if (this->hold_off_until_ != 0 && millis() < this->hold_off_until_)
    return true;
  this->attempt_connect_(false);
  return true;
}

void BLEClient::connect() {
  if (this->state_ != State::IDLE)
    return;
  // Without a fresh sighting each attempt can inhibit scanning for the
  // backend's full connect timeout (20 s on rp2) if the peer is absent.
  ESP_LOGW(TAG, "[%s] Connecting without a recent advertisement", this->address_str_);
  this->attempt_connect_(true);
}

void BLEClient::attempt_connect_(bool from_action) {
  int err = this->backend_->connect(this->address_, this->address_type_);
  if (err != 0) {
    // A refused connect never produces a callback; stay idle and let the
    // next sighting (or action) retry.
    ESP_LOGW(TAG, "[%s] Connect refused, err=%d", this->address_str_, err);
    return;
  }
  ESP_LOGD(TAG, "[%s] Connecting", this->address_str_);
  this->state_ = State::CONNECTING;
}

void BLEClient::disconnect() {
  if (this->state_ == State::IDLE)
    return;
  this->backend_->disconnect();
}

void BLEClient::request_notify(uint16_t char_handle, uint16_t cccd_handle, uint8_t properties) {
  if (this->cccd_count_ >= BLE_CLIENT_MAX_NOTIFY_REQUESTS) {
    ESP_LOGW(TAG, "[%s] Too many notify requests, dropping handle 0x%04x", this->address_str_, char_handle);
    return;
  }
  // Local registration is free (no operation slot); the CCCD write is
  // serialized through the client's queue because the backend accepts one
  // outstanding operation at a time.
  this->backend_->notify_characteristic(char_handle, true);
  uint8_t value = (properties & ble_device_base::GATT_CHAR_PROP_NOTIFY) ? 1 : 2;
  this->pending_cccd_[this->cccd_count_++] = {cccd_handle, value};
}

void BLEClient::issue_next_cccd_() {
  while (this->cccd_head_ < this->cccd_count_) {
    const auto &pending = this->pending_cccd_[this->cccd_head_];
    const uint8_t value[2] = {pending.value, 0x00};
    if (this->backend_->write_descriptor(pending.handle, value, sizeof(value)) == 0) {
      return;  // advance on the matching on_write_result
    }
    ESP_LOGW(TAG, "[%s] CCCD write failed for handle 0x%04x", this->address_str_, pending.handle);
    this->cccd_head_++;
  }
}

void BLEClient::register_failure_() {
  if (this->consecutive_failures_ < FAILURE_HOLD_OFF_MAX_STEPS)
    this->consecutive_failures_++;
  this->hold_off_until_ = millis() + this->consecutive_failures_ * FAILURE_HOLD_OFF_STEP_MS;
  ESP_LOGW(TAG, "[%s] Holding off reconnect for %u s", this->address_str_,
           this->consecutive_failures_ * (FAILURE_HOLD_OFF_STEP_MS / 1000));
}

void BLEClient::on_connection_state(bool connected, uint16_t mtu, int error) {
  if (connected) {
    this->state_ = State::DISCOVERING;
    if (this->backend_->discover_services() != 0) {
      // Synchronous refusal: no discovery completion will follow.
      this->backend_->disconnect();
    }
    return;
  }
  bool was_connected = this->state_ == State::CONNECTED;
  this->state_ = State::IDLE;
  this->cccd_head_ = this->cccd_count_ = 0;
  if (was_connected) {
    ESP_LOGI(TAG, "[%s] Disconnected, status=%d", this->address_str_, error);
    for (auto *node : this->nodes_) {
      node->on_disconnected();
    }
    // Continuations leave the backend's event-drain stack first.
    this->defer([this]() { this->disconnect_callbacks_.call(); });
  } else {
    ESP_LOGW(TAG, "[%s] Connect failed, status=%d", this->address_str_, error);
    this->register_failure_();
    this->defer([this]() { this->connect_failed_callbacks_.call(); });
  }
}

void BLEClient::on_service_discovery_done(int error) {
  if (error != 0) {
    ESP_LOGW(TAG, "[%s] Service discovery failed, status=%d", this->address_str_, error);
    this->register_failure_();
    this->backend_->disconnect();
    return;
  }
  auto table = this->backend_->get_service_table();
  for (auto *node : this->nodes_) {
    node->on_connected(table);
  }
  this->backend_->release_services();
  this->state_ = State::CONNECTED;
  this->consecutive_failures_ = 0;
  this->hold_off_until_ = 0;
  ESP_LOGI(TAG, "[%s] Connected", this->address_str_);
  this->defer([this]() { this->connect_callbacks_.call(); });
  this->issue_next_cccd_();
}

void BLEClient::on_write_result(uint16_t handle, int error) {
  // The client's CCCD queue claims its own completion before the node
  // fan-out sees it.
  if (this->cccd_head_ < this->cccd_count_ && this->pending_cccd_[this->cccd_head_].handle == handle) {
    if (error != 0) {
      ESP_LOGW(TAG, "[%s] CCCD write error %d on handle 0x%04x", this->address_str_, error, handle);
    }
    this->cccd_head_++;
    this->issue_next_cccd_();
    return;
  }
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
