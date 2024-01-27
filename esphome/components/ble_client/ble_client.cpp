#include "ble_client.h"
#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_client {

static const char *const TAG = "ble_client";

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
  ESP_LOGCONFIG(TAG, "  Address: %s", this->address_str().c_str());
  ESP_LOGCONFIG(TAG, "  Auto-Connect: %s", TRUEFALSE(this->auto_connect_));
}

bool BLEClient::parse_device(const espbt::ESPBTDevice &device) {
  if (!this->enabled)
    return false;
  return BLEClientBase::parse_device(device);
}

void BLEClient::set_enabled(bool enabled) {
  if (enabled == this->enabled)
    return;
  this->enabled = enabled;
  if (!enabled) {
    ESP_LOGI(TAG, "[%s] Disabling BLE client.", this->address_str().c_str());
    this->disconnect();
  }
}

bool BLEClient::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t esp_gattc_if,
                                    esp_ble_gattc_cb_param_t *param) {
  if (!BLEClientBase::gattc_event_handler(event, esp_gattc_if, param))
    return false;

  for (auto *node : this->nodes_)
    node->gattc_event_handler(event, esp_gattc_if, param);

  if (!this->services_.empty() && this->all_nodes_established_()) {
    this->release_services();
    ESP_LOGD(TAG, "All clients established, services released");
  }
  return true;
}

void BLEClient::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  BLEClientBase::gap_event_handler(event, param);

  for (auto *node : this->nodes_)
    node->gap_event_handler(event, param);
}

void BLEClient::set_state(espbt::ClientState state) {
  BLEClientBase::set_state(state);
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

}  // namespace ble_client
}  // namespace esphome

#endif
