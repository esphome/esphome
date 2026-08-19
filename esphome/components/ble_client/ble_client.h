#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_CLIENT_LEGACY_ENGINE

#include "ble_client_node.h"
#include "connect_backoff.h"
#include "esphome/components/esp32_ble_client/ble_client_base.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>
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
  int check_and_log_error_(const char *operation, esp_err_t err);
  int find_pending_gatt_reg_(uint16_t handle) const;
  void notify_state_to_gatt_nodes_(uint16_t handle, bool enabled, int error);
  void dispatch_gatt_event_(esp_gattc_cb_event_t event, esp_ble_gattc_cb_param_t *param);
  // False = failed discovery: the link comes down and the caller suppresses
  // the legacy fan-out.
  bool handle_gatt_search_cmpl_(esp_gatt_status_t status);
  bool take_pending_gatt_reg_(uint16_t handle);
  void on_disconnect_complete(esp_err_t reason) override;
#endif

  std::vector<BLEClientNode *> nodes_;
#ifdef USE_BLE_CLIENT_GATT_NODES
  // Raise if a migrated node needs more concurrent registrations.
  static constexpr uint8_t MAX_PENDING_NOTIFY_REGS = 4;

  // Nodes on the neutral surface; fed the translated callbacks and
  // auto-established after the on_connected fan-out. Every gatt node is
  // also in nodes_ (registration pushes into both).
  StaticVector<BLEClientNode *, ESPHOME_BLE_CLIENT_MAX_NODES> gatt_nodes_;
  bool has_legacy_nodes_() const { return this->nodes_.size() > this->gatt_nodes_.size(); }
  // Reconnect backoff after materializer failures.
  ConnectBackoff gatt_backoff_;
  // Bridge-initiated notify registrations awaiting REG_FOR_NOTIFY_EVT.
  uint16_t pending_gatt_regs_[MAX_PENDING_NOTIFY_REGS];
  uint8_t pending_gatt_reg_count_{0};
  // on_connected fan-out started; on_disconnected is owed at teardown.
  bool gatt_connected_{false};
#endif
};

}  // namespace esphome::ble_client

#endif
