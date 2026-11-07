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
#include <cstdint>
#include "esphome/core/log.h"

namespace esphome::esp32_ble_tracker {
static const char *const TAG = "esp32_ble_tracker";
}  // namespace esphome::esp32_ble_tracker

static_assert(ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 2, 0),
              "ESP-IDF 6.2 and later have the fix, this guard should not be enabled (esphome/esphome#19373)");

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

bool __real_l2cble_init_direct_conn(void *p_lcb);
void l2cu_release_lcb(void *p_lcb);

bool __wrap_l2cble_init_direct_conn(void *p_lcb) {
  // in_use is the first member of the private tL2C_LCB (checked ESP-IDF 5.0 to 6.1)
  const auto *in_use = static_cast<const uint8_t *>(p_lcb);
  if (p_lcb == nullptr || *in_use == 0) {
    ESP_LOGW(esphome::esp32_ble_tracker::TAG, "Dropped queued connect on a released link block");
    return false;
  }
  const bool started = __real_l2cble_init_direct_conn(p_lcb);
  // Every failure path releases the block except unknown device, also fixed upstream
  if (!started && *in_use != 0) {
    l2cu_release_lcb(p_lcb);
  }
  return started;
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP32_BLE_TRACKER_DIRECT_CONN_GUARD
