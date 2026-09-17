/*
 * Linker wrap guard for Bluedroid's queued BLE connection requests.
 *
 * Bluedroid starts one outgoing BLE connection at a time and parks the rest
 * as raw link control block pointers. A parked request is not dropped when
 * its block is released, so btm_send_pending_direct_conn() can later pass a
 * released block to l2cble_init_direct_conn(). That arms the link timer with
 * a null parameter, and l2c_link_timeout() crashes when it expires.
 *
 * The same commit also releases a live block that l2cble_init_direct_conn()
 * rejects on its unknown device path, which every other failure path already
 * does; the wrapper releases it when the block is still in use afterwards.
 *
 * The wrap only covers calls from other files, which is where the stale
 * pointer comes from; callers inside l2c_ble.c pass a live block.
 *
 * Fixed upstream in espressif/esp-idf commit 82e71c1767. Codegen only enables
 * this guard for ESP-IDF releases without that commit; remove it once the
 * minimum ESP-IDF includes it.
 */

#include "esphome/core/defines.h"

#ifdef USE_ESP32_BLE_TRACKER_DIRECT_CONN_GUARD

#include <esp_idf_version.h>

namespace esphome::esp32_ble_tracker {}

// tL2C_LCB is private to Bluedroid, so its layout was checked by hand from ESP-IDF 5.0 to 6.1:
// in_use is the first member and BOOLEAN is bool.
static_assert(ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 2, 0),
              "ESP-IDF 6.2 and later have the fix, this guard should not be enabled (esphome/esphome#19373)");
static_assert(sizeof(bool) == 1, "in_use is read as a single byte");

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

bool __real_l2cble_init_direct_conn(void *p_lcb);
void l2cu_release_lcb(void *p_lcb);

bool __wrap_l2cble_init_direct_conn(void *p_lcb) {
  if (p_lcb == nullptr || !*static_cast<const bool *>(p_lcb)) {
    return false;
  }
  const bool started = __real_l2cble_init_direct_conn(p_lcb);
  if (!started && *static_cast<const bool *>(p_lcb)) {
    l2cu_release_lcb(p_lcb);
  }
  return started;
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP32_BLE_TRACKER_DIRECT_CONN_GUARD
