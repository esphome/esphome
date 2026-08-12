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

#ifdef USE_BLE_CLIENT_GATT_NODES
  // ---- the neutral node surface (signatures shared with the non-esp32
  // engine, so nodes on the neutral interface compile against either) ----
  void register_gatt_node(BLEClientNode *node);

  bool idle() const { return this->state() == espbt::ClientState::IDLE; }

  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response);
  int read_characteristic(uint16_t handle);
  int read_descriptor(uint16_t handle);
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len);
  /// Local registration only; per the neutral contract the CCCD write is the
  /// node's job (the legacy auto-CCCD is suppressed for these handles).
  int notify_characteristic(uint16_t handle, bool enable);
  // pair() comes from BLEClientBase, matching the neutral engine's.
  int unpair();
#endif

 protected:
  bool all_nodes_established_();
  void maybe_release_services_();
#ifdef USE_BLE_CLIENT_GATT_NODES
  int check_gatt_op_(const char *operation, esp_err_t err);
  void register_gatt_failure_();
  bool dispatch_gatt_event_(esp_gattc_cb_event_t event, esp_ble_gattc_cb_param_t *param);
  bool handle_gatt_search_cmpl_(esp_gatt_status_t status);
  bool take_pending_gatt_reg_(uint16_t handle);
  void on_disconnect_complete(esp_err_t reason) override;
#endif

  std::vector<BLEClientNode *> nodes_;
#ifdef USE_BLE_CLIENT_GATT_NODES
  // Nodes on the neutral surface; fed the translated callbacks and
  // auto-established after the on_connected fan-out.
  StaticVector<BLEClientNode *, ESPHOME_BLE_CLIENT_MAX_NODES> gatt_nodes_;
  // Bridge-initiated notify registrations awaiting REG_FOR_NOTIFY_EVT.
  uint16_t pending_gatt_regs_[ESPHOME_BLE_CLIENT_MAX_NODES];
  uint8_t pending_gatt_reg_count_{0};
  // on_connected fan-out started; on_disconnected is owed at teardown.
  bool gatt_connected_{false};
  // Reconnect backoff after materializer failures (wrap-safe start+duration).
  uint32_t gatt_hold_off_start_{0};
  uint32_t gatt_hold_off_ms_{0};
  uint8_t gatt_consecutive_failures_{0};
#endif
};

}  // namespace esphome::ble_client

#endif
