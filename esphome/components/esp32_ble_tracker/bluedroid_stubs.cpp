/*
 * Bluedroid queues outgoing BLE connections as raw link block pointers and does
 * not drop them when the block is released, so btm_send_pending_direct_conn()
 * can start a connect on a released block and l2c_link_timeout() later crashes
 * on its null timer parameter. Mirrors espressif/esp-idf@82e71c1767; codegen
 * only enables it for releases without that commit.
 */

#include "esphome/core/defines.h"

#ifdef USE_ESP32_BLE_TRACKER_DIRECT_CONN_GUARD

#include <esp_idf_version.h>

namespace esphome::esp32_ble_tracker {}

// in_use is the first member of the private tL2C_LCB (checked ESP-IDF 5.0 to 6.1)
static_assert(ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 2, 0),
              "ESP-IDF 6.2 and later have the fix, this guard should not be enabled (esphome/esphome#19373)");

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

bool __real_l2cble_init_direct_conn(void *p_lcb);
void l2cu_release_lcb(void *p_lcb);

bool __wrap_l2cble_init_direct_conn(void *p_lcb) {
  if (p_lcb == nullptr || !*static_cast<const bool *>(p_lcb)) {
    return false;
  }
  const bool started = __real_l2cble_init_direct_conn(p_lcb);
  // Every failure path releases the block except unknown device, also fixed upstream
  if (!started && *static_cast<const bool *>(p_lcb)) {
    l2cu_release_lcb(p_lcb);
  }
  return started;
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP32_BLE_TRACKER_DIRECT_CONN_GUARD
