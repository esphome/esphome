#pragma once

#ifdef USE_ESP32

#include <cstdint>
#include <span>

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/genibus/genibus.h"

#include "link_state.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

namespace esphome::genibus_ble {

namespace espbt = esphome::esp32_ble_tracker;

/// A GENIbus master over Bluetooth LE, as spoken by the Grundfos ALPHA pumps and the Grundfos GO app.
/// The link counts as connected only once notifications of the GENI characteristic are enabled and it has settled.
/// Whether the pump is seen at all is left to ble_client: it scans quietly and connects when it sees it.
class GenibusBLE final : public genibus::GenibusBus, public ble_client::BLEClientNode {
 public:
  void dump_config() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

  bool is_connected() const override { return this->link_.is_up(); }

 protected:
  static constexpr uint32_t SETTLE_TIMEOUT_ID = 0;

  bool write_frame(std::span<const uint8_t> frame) override;
  void link_up_();
  void link_down_();
  void link_fault_(const LogString *reason);

  LinkState link_;
};

}  // namespace esphome::genibus_ble

#endif  // USE_ESP32
