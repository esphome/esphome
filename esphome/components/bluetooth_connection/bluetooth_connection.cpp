#include "bluetooth_connection.h"

#ifdef USE_ESP32
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#endif

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS

#include "esphome/components/api/api_pb2.h"
#include "esphome/core/log.h"

namespace esphome::bluetooth_connection {

static const char *const TAG = "bluetooth_connection";

BatchClose close_service_batch(api::BluetoothGATTGetServicesResponse &resp, size_t &current_size, int16_t &send_service,
                               uint8_t connection_index, const char *address_str) {
  // Calculate the actual size of just this service (+1 for the field tag)
  size_t service_size = resp.services.back().calculate_size() + 1;

  if (current_size + service_size > MAX_PACKET_SIZE) {
    if (resp.services.size() > 1) {
      // We would go over -- pop the last service and retry it in the next batch
      resp.services.pop_back();
      ESP_LOGD(TAG, "[%d] [%s] Service %d would exceed limit (current: %u + service: %u > %u), sending current batch",
               connection_index, address_str, send_service, (unsigned) current_size, (unsigned) service_size,
               (unsigned) MAX_PACKET_SIZE);
      // Don't advance send_service -- the popped service goes into the next batch
    } else {
      // This single service is too large, but we have to send it anyway;
      // advance so we don't get stuck
      ESP_LOGW(TAG, "[%d] [%s] Service %d is too large (%u bytes) but sending anyway", connection_index, address_str,
               send_service, (unsigned) service_size);
      send_service++;
    }
    return BatchClose::SEND;
  }

  current_size += service_size;
  send_service++;
  return BatchClose::CONTINUE;
}

}  // namespace esphome::bluetooth_connection

#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS

#if defined(USE_ESP32) && defined(USE_BLE_GATT_CLIENT)
namespace esphome::bluetooth_connection {

// Address-scoped Bluedroid maintenance. Gated with the connection surface:
// the advertisement-only arm no longer dispatches these requests at all.

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
#endif  // USE_ESP32 && USE_BLE_GATT_CLIENT
