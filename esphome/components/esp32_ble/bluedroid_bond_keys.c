// Linker wraps that keep a bonded device's keys when it re-encrypts without pairing again (ESP-IDF 5.5.6).
// Same guard as espressif/esp-idf#19322; drop once ESPHome uses an ESP-IDF release that contains it.
// C, because Bluedroid's btm_ble_int.h does not close its extern "C" block.

// Compiled only with Bluedroid's private headers (bluedroid_headers_cmake); static analysis skips it
#ifdef USE_ESP32_BLE_BLUEDROID_HEADERS

#include <stdbool.h>
#include <stddef.h>

#include "btm_int.h"
#include "smp_int.h"

// The device record whose keys smp_check_auth_req() must keep, or NULL
static tBTM_SEC_DEV_REC *keep_keys_of = NULL;

void __real_smp_check_auth_req(tSMP_CB *p_cb, tSMP_INT_DATA *p_data);
void __real_btm_sec_clear_ble_keys(tBTM_SEC_DEV_REC *p_dev_rec);

// Runs on the Bluetooth host task. Encryption that succeeded without a Pairing Request or Response reused the bond.
void __wrap_smp_check_auth_req(tSMP_CB *p_cb, tSMP_INT_DATA *p_data) {
  const bool reused_bond = *(UINT8 *) p_data == 1 && !(p_cb->flags & SMP_PAIR_FLAG_ENC_AFTER_PAIR);
  // The same lookup as smp_check_auth_req(): it also matches a pseudo or resolvable private address
  keep_keys_of = reused_bond ? btm_find_dev(p_cb->pairing_bda) : NULL;
  __real_smp_check_auth_req(p_cb, p_data);
  keep_keys_of = NULL;
}

void __wrap_btm_sec_clear_ble_keys(tBTM_SEC_DEV_REC *p_dev_rec) {
  // Only the first clear, the one the upstream guard skips; later callers are not affected
  if (keep_keys_of != NULL && p_dev_rec == keep_keys_of) {
    keep_keys_of = NULL;
    return;
  }
  __real_btm_sec_clear_ble_keys(p_dev_rec);
}

#endif  // USE_ESP32_BLE_BLUEDROID_HEADERS
