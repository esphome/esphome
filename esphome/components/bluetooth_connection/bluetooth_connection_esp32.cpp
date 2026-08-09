// Address-scoped Bluedroid maintenance shared by every esp32 proxy build,
// including advertisement-only ones where no GATT backend is compiled.

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "bluetooth_connection.h"

#include <esp_bt_device.h>
#include <esp_gattc_api.h>

namespace esphome::bluetooth_connection {

conn_err_t unpair_device(uint64_t address) {
  esp_bd_addr_t bda;
  ble_device_base::uint64_to_mac_msb_first(address, bda);
  return esp_ble_remove_bond_device(bda);
}

conn_err_t clear_gatt_cache(uint64_t address) {
  esp_bd_addr_t bda;
  ble_device_base::uint64_to_mac_msb_first(address, bda);
  return esp_ble_gattc_cache_clean(bda);
}

}  // namespace esphome::bluetooth_connection

#endif  // USE_ESP32
