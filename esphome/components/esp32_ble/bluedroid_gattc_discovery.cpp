// Linker wrap that keeps the status of a failed GATT discovery, which Bluedroid drops: bta_gattc_disc_cmpl_cback()
// stores only a successful status, so the search result reports a partial table as complete (esphome#22518).
// gattc_event_handler() puts the status back into ESP_GATTC_SEARCH_CMPL_EVT.

#include "esphome/core/defines.h"

#ifdef USE_ESP32_BLE_GATTC_DISCOVERY_STATUS

#include "bluedroid_gattc_discovery.h"

namespace esphome::esp32_ble {

static DiscoveryErrors<USE_ESP32_BLE_MAX_CONNECTIONS> discovery_errors;  // NOLINT

uint8_t take_discovery_error(uint16_t conn_id) { return discovery_errors.take(conn_id); }

}  // namespace esphome::esp32_ble

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

// tGATT_DISC_TYPE and tGATT_STATUS are UINT8
void __real_bta_gattc_disc_cmpl_cback(uint16_t conn_id, uint8_t disc_type, uint8_t status);

// Runs on the Bluetooth host task, before the search result is sent
void __wrap_bta_gattc_disc_cmpl_cback(uint16_t conn_id, uint8_t disc_type, uint8_t status) {
  if (status != 0)
    esphome::esp32_ble::discovery_errors.record(conn_id, status);
  __real_bta_gattc_disc_cmpl_cback(conn_id, disc_type, status);
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP32_BLE_GATTC_DISCOVERY_STATUS
