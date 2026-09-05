// Replaces the gatt_client / hci_connection static pools baked into
// arduino-pico's prebuilt liblwip-bt.a (built with MAX_NR_GATT_CLIENTS 1,
// MAX_NR_HCI_CONNECTIONS 2) with pools sized from ESPHOME_BLE_GATT_CLIENT_COUNT.
// add_btstack_pool_overrides() in this component's codegen emits the matching
// -Wl,--wrap flags, requested by bluetooth_connection when more than one GATT
// backend registers; single-backend builds emit no flags and this file
// compiles to nothing, leaving the prebuilt pools in charge. Layout safety:
// the framework defines ENABLE_CLASSIC / ENABLE_BLE for every user TU
// whenever PIO_FRAMEWORK_ARDUINO_ENABLE_BLUETOOTH is set (this component
// always sets it), so sizeof() here matches the archive.

#include "esphome/core/defines.h"

#if defined(USE_RP2040_BLE) && defined(USE_BLE_GATT_CLIENT) && (ESPHOME_BLE_GATT_CLIENT_COUNT > 1)

#include <btstack.h>

#include <cstring>

namespace esphome::rp2040_ble {
namespace {

// Pinned against arduino-pico 6.0.0's prebuilt archives: a framework bump (or
// a changed ENABLE_* macro) shifting the struct layout must fail the build
// here, not overrun the pool blocks at runtime. Sizes differ per core
// architecture (measured from each archive's own storage symbols). GCC only:
// the clang-tidy frontend lays these structs out differently, and the guard
// targets the real link.
#ifndef __clang__
#ifdef __riscv
static_assert(sizeof(gatt_client_t) == 140 && sizeof(hci_connection_t) == 3740, "BTstack layout changed");
#else
static_assert(sizeof(gatt_client_t) == 128 && sizeof(hci_connection_t) == 3688, "BTstack layout changed");
#endif
#endif  // __clang__

// One gatt_client_t per configured connection slot. An hci_connection_t is
// held from gap_connect() to DISCONNECTION_COMPLETE (scanning holds none);
// +1 mirrors the prebuilt library's own headroom (2 connections for 1 GATT
// client) so a teardown/re-connect overlap can never starve a slot.
constexpr int HCI_CONNECTION_POOL_SIZE = ESPHOME_BLE_GATT_CLIENT_COUNT + 1;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp)
gatt_client_t gatt_client_storage[ESPHOME_BLE_GATT_CLIENT_COUNT];
btstack_memory_pool_t gatt_client_pool;
hci_connection_t hci_connection_storage[HCI_CONNECTION_POOL_SIZE];
btstack_memory_pool_t hci_connection_pool;

// Static init: pool_create only links a free list through its own storage,
// and BTstack first allocates long after static construction.
struct PoolInit {
  PoolInit() {
    btstack_memory_pool_create(&gatt_client_pool, gatt_client_storage, ESPHOME_BLE_GATT_CLIENT_COUNT,
                               sizeof(gatt_client_t));
    btstack_memory_pool_create(&hci_connection_pool, hci_connection_storage, HCI_CONNECTION_POOL_SIZE,
                               sizeof(hci_connection_t));
  }
} pool_init;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp)

}  // namespace

// Exact semantics of btstack_memory.c's static-pool arm: zeroed block on
// success, NULL when exhausted; free returns the block to the pool. The
// prebuilt pools stay resident in .bss (~7.4 KB, kept live by
// btstack_memory_init in the archive) — dead weight here, not a leak.
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" gatt_client_t *__real_btstack_memory_gatt_client_get(void);
extern "C" void __real_btstack_memory_gatt_client_free(gatt_client_t *gatt_client);
extern "C" hci_connection_t *__real_btstack_memory_hci_connection_get(void);
extern "C" void __real_btstack_memory_hci_connection_free(hci_connection_t *hci_connection);

namespace {
// Fails the link if the corresponding --wrap flag is missing: __real_* only
// exists while --wrap is in effect, and each wrap function anchors its own
// symbol so dropping any single flag fails loudly. A code reference is used
// because the framework links with --gc-sections, which discards an
// unreferenced data anchor regardless of [[gnu::used]] (and this toolchain
// does not emit SHF_GNU_RETAIN for [[gnu::retain]]).
template<typename T> void anchor_wrap(T *symbol) { asm volatile("" ::"r"(symbol)); }
}  // namespace

extern "C" {

gatt_client_t *__wrap_btstack_memory_gatt_client_get(void) {
  anchor_wrap(&__real_btstack_memory_gatt_client_get);
  void *buffer = btstack_memory_pool_get(&gatt_client_pool);
  if (buffer != nullptr) {
    memset(buffer, 0, sizeof(gatt_client_t));
  }
  return static_cast<gatt_client_t *>(buffer);
}

void __wrap_btstack_memory_gatt_client_free(gatt_client_t *gatt_client) {
  anchor_wrap(&__real_btstack_memory_gatt_client_free);
  btstack_memory_pool_free(&gatt_client_pool, gatt_client);
}

hci_connection_t *__wrap_btstack_memory_hci_connection_get(void) {
  anchor_wrap(&__real_btstack_memory_hci_connection_get);
  void *buffer = btstack_memory_pool_get(&hci_connection_pool);
  if (buffer != nullptr) {
    memset(buffer, 0, sizeof(hci_connection_t));
  }
  return static_cast<hci_connection_t *>(buffer);
}

void __wrap_btstack_memory_hci_connection_free(hci_connection_t *hci_connection) {
  anchor_wrap(&__real_btstack_memory_hci_connection_free);
  btstack_memory_pool_free(&hci_connection_pool, hci_connection);
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

}  // namespace esphome::rp2040_ble

#endif  // USE_RP2040_BLE && USE_BLE_GATT_CLIENT && ESPHOME_BLE_GATT_CLIENT_COUNT > 1
