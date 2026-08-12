#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "ble_client_node.h"
#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>
#include <array>
#include <string>
#include <vector>

namespace esphome::ble_client {

namespace espbt = esphome::esp32_ble_tracker;

using namespace esp32_ble_client;

class BLEClient final : public BLEClientBase {
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
    node->set_ble_client_parent(this);
    this->nodes_.push_back(node);
  }

  bool enabled;

  void set_state(espbt::ClientState state) override;

 protected:
  bool all_nodes_established_();

  std::vector<BLEClientNode *> nodes_;
};

}  // namespace esphome::ble_client

#endif
