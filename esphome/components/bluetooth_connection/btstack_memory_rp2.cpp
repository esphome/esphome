// Replaces the gatt_client / hci_connection static pools baked into
// arduino-pico's prebuilt liblwip-bt.a (built with MAX_NR_GATT_CLIENTS 1,
// MAX_NR_HCI_CONNECTIONS 2) with pools sized from ESPHOME_BLE_GATT_CLIENT_COUNT.
// bluetooth_connection/__init__.py emits the matching -Wl,--wrap flags only
// when more than one connection slot is configured; single-slot builds emit no
// flags and this file compiles to nothing, leaving the prebuilt pools in
// charge. Layout safety: the framework defines ENABLE_CLASSIC / ENABLE_BLE for
// every user TU whenever PIO_FRAMEWORK_ARDUINO_ENABLE_BLUETOOTH is set
// (rp2040_ble always sets it), so sizeof() here matches the archive.

#include "esphome/core/defines.h"

#if defined(USE_RP2040_BLE) && defined(USE_BLE_GATT_CLIENT) && (ESPHOME_BLE_GATT_CLIENT_COUNT > 1)

#include <btstack.h>

#include <cstring>

namespace esphome::bluetooth_connection {
namespace {

// One gatt_client_t per configured connection slot.
constexpr int GATT_CLIENT_POOL_SIZE = ESPHOME_BLE_GATT_CLIENT_COUNT;
// An hci_connection_t is held from gap_connect() to DISCONNECTION_COMPLETE
// (scanning holds none); +1 mirrors the prebuilt library's own headroom
// (2 connections for 1 GATT client) so a teardown/re-connect overlap can
// never starve a slot.
constexpr int HCI_CONNECTION_POOL_SIZE = ESPHOME_BLE_GATT_CLIENT_COUNT + 1;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
gatt_client_t gatt_client_storage[GATT_CLIENT_POOL_SIZE];
btstack_memory_pool_t gatt_client_pool;
hci_connection_t hci_connection_storage[HCI_CONNECTION_POOL_SIZE];
btstack_memory_pool_t hci_connection_pool;
bool pools_ready = false;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// Lazy one-time init instead of a global constructor: every caller (hci.c /
// gatt_client.c inside the prebuilt archive) runs in BTstack's single
// serialized context, so a plain flag is race-free and no static-init-order
// hazard exists.
void ensure_pools() {
  if (pools_ready)
    return;
  btstack_memory_pool_create(&gatt_client_pool, gatt_client_storage, GATT_CLIENT_POOL_SIZE, sizeof(gatt_client_t));
  btstack_memory_pool_create(&hci_connection_pool, hci_connection_storage, HCI_CONNECTION_POOL_SIZE,
                             sizeof(hci_connection_t));
  pools_ready = true;
}

}  // namespace

// Exact semantics of btstack_memory.c's static-pool arm: zeroed block on
// success, NULL when exhausted; free returns the block to the pool.
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

gatt_client_t *__wrap_btstack_memory_gatt_client_get(void) {
  ensure_pools();
  void *buffer = btstack_memory_pool_get(&gatt_client_pool);
  if (buffer != nullptr) {
    memset(buffer, 0, sizeof(gatt_client_t));
  }
  return static_cast<gatt_client_t *>(buffer);
}

void __wrap_btstack_memory_gatt_client_free(gatt_client_t *gatt_client) {
  btstack_memory_pool_free(&gatt_client_pool, gatt_client);
}

hci_connection_t *__wrap_btstack_memory_hci_connection_get(void) {
  ensure_pools();
  void *buffer = btstack_memory_pool_get(&hci_connection_pool);
  if (buffer != nullptr) {
    memset(buffer, 0, sizeof(hci_connection_t));
  }
  return static_cast<hci_connection_t *>(buffer);
}

void __wrap_btstack_memory_hci_connection_free(hci_connection_t *hci_connection) {
  btstack_memory_pool_free(&hci_connection_pool, hci_connection);
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

}  // namespace esphome::bluetooth_connection

#endif  // USE_RP2040_BLE && USE_BLE_GATT_CLIENT && ESPHOME_BLE_GATT_CLIENT_COUNT > 1
