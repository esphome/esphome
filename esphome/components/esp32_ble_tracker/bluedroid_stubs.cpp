/*
 * Linker wrap guard for Bluedroid's queued BLE connection requests.
 *
 * Bluedroid starts one outgoing BLE connection at a time and parks the rest
 * as raw link control block pointers. A parked request is not dropped when
 * its block is released, so btm_send_pending_direct_conn() can later pass a
 * released block to l2cble_init_direct_conn(). That arms the link timer with
 * a null parameter, and l2c_link_timeout() crashes when it expires.
 *
 * Fixed upstream in espressif/esp-idf commit 82e71c1767, which is not in a
 * tagged release yet. Remove once the minimum ESP-IDF includes it.
 */

#include "esphome/core/defines.h"

#ifdef USE_ESP32_BLE_TRACKER_DIRECT_CONN_GUARD

namespace esphome::esp32_ble_tracker {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

bool __real_l2cble_init_direct_conn(void *p_lcb);

bool __wrap_l2cble_init_direct_conn(void *p_lcb) {
  // in_use (bool) is the first member of tL2C_LCB, checked from ESP-IDF 5.3.2 to 6.1
  if (p_lcb == nullptr || !*static_cast<const bool *>(p_lcb)) {
    return false;
  }
  return __real_l2cble_init_direct_conn(p_lcb);
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP32_BLE_TRACKER_DIRECT_CONN_GUARD
