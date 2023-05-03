#pragma once

#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>
#include <array>
#include <string>
#include <vector>

namespace esphome {
namespace ble_client {

namespace espbt = esphome::esp32_ble_tracker;

using namespace esp32_ble_client;

class BLEClient;

class BLEClientNode {
 public:
  virtual void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                   esp_ble_gattc_cb_param_t *param){};
  virtual void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {}
  virtual void loop() {}
  void set_address(uint64_t address) { address_ = address; }
  espbt::ESPBTClient *client;
  // This should be transitioned to Established once the node no longer needs
  // the services/descriptors/characteristics of the parent client. This will
  // allow some memory to be freed.
  espbt::ClientState node_state;

  BLEClient *parent() { return this->parent_; }
  void set_ble_client_parent(BLEClient *parent) { this->parent_ = parent; }

 protected:
  BLEClient *parent_;
  uint64_t address_;
};

class BLEClient : public BLEClientBase {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  bool parse_device(const espbt::ESPBTDevice &device) override;

  void set_enabled(bool enabled);

  void register_ble_node(BLEClientNode *node) {
    node->client = this;
    node->set_ble_client_parent(this);
    this->nodes_.push_back(node);
  }

  bool enabled;

  void set_state(espbt::ClientState state) override;

 protected:
  bool all_nodes_established_();

  std::vector<BLEClientNode *> nodes_;
};

}  // namespace ble_client
}  // namespace esphome

#endif
