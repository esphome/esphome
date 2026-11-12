// Linker wrap that puts a GATT discovery response back in handle order before Bluedroid parses it.
// ESP-IDF 5.5.6 and 6.0.3 end the whole discovery when a peer lists handles out of order (esphome#22518), which
// earlier releases, Android and BlueZ accept.

#include "esphome/core/defines.h"

#ifdef USE_ESP32_BLE_GATTC_HANDLE_ORDER

#include "bluedroid_gattc_order.h"

namespace esphome::esp32_ble {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

// tGATT_TCB is only passed through
void __real_gatt_client_handle_server_rsp(void *p_tcb, uint8_t op_code, uint16_t len, uint8_t *p_data,
                                          uint16_t eatt_bearer_lcid);

// Runs on the Bluetooth host task
void __wrap_gatt_client_handle_server_rsp(void *p_tcb, uint8_t op_code, uint16_t len, uint8_t *p_data,
                                          uint16_t eatt_bearer_lcid) {
  esphome::esp32_ble::sort_att_response_by_handle(op_code, p_data, len);
  __real_gatt_client_handle_server_rsp(p_tcb, op_code, len, p_data, eatt_bearer_lcid);
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#endif  // USE_ESP32_BLE_GATTC_HANDLE_ORDER
