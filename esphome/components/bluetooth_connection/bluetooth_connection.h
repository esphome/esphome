// Shared types and helpers for the per-platform GATT connection backends and
// the Bluetooth proxy that drives them.

#pragma once

#include "esphome/core/defines.h"

#include "esphome/components/ble_device_base/ble_client_state.h"
#include "esphome/components/ble_device_base/ble_device.h"

#include <array>
#include <cstddef>
#include <cstdint>

#ifdef USE_ESP32
#include <esp_err.h>
#endif

// USE_BLUETOOTH_PROXY_CONNECTIONS is the single spelling of "this build has
// proxy connection slots": codegen emits it per configured slot, and each
// slot brings a GATT backend, so it also implies USE_BLE_GATT_CLIENT (not
// the converse: a backend can exist without proxy slots). The hub
// wrapper, the proxy's connection surface and the API's connection messages
// all gate on it. The address-scoped maintenance functions below are only
// reached from that gated surface; the #else stubs just keep this header
// parsing on arms without a backend.

namespace esphome::api {
class BluetoothGATTGetServicesResponse;
}  // namespace esphome::api

namespace esphome::bluetooth_connection {

// Connection-owned error type for the API error fields, which are plain
// integers on the wire. Aliases esp_err_t on esp32 (where the values come from
// IDF calls); a bare int elsewhere. Owning the name instead of probing for
// esp_err_t keeps the header independent of how a platform's SDK spells its
// error type.
#ifdef USE_ESP32
using conn_err_t = esp_err_t;
static constexpr conn_err_t CONN_OK = ESP_OK;
#else
using conn_err_t = int;
static constexpr conn_err_t CONN_OK = 0;
#endif

// The ESPHome-private "not connected" wire value, shared with the neutral
// GATT contract so backend and wrapper cannot drift.
static constexpr conn_err_t GATT_NOT_CONNECTED = ble_device_base::GATT_ERR_NOT_CONNECTED;

// What the platform's connection backend supports beyond GATT operations;
// the proxy derives its feature flags and legacy version from these.
#if defined(USE_ESP32)
static constexpr bool SUPPORTS_PAIRING = true;
static constexpr bool SUPPORTS_CACHE_CLEARING = true;
#elif defined(USE_RP2040_BLE) && defined(USE_BLE_GATT_CLIENT)
// The rp2 BTstack backend pairs (just works + bonding); it has no service
// cache to clear. Keyed on the backend, not the generic client define, so a
// future backend without pairing keeps the stub arm below.
static constexpr bool SUPPORTS_PAIRING = true;
static constexpr bool SUPPORTS_CACHE_CLEARING = false;
#else
static constexpr bool SUPPORTS_PAIRING = false;
static constexpr bool SUPPORTS_CACHE_CLEARING = false;
#endif

// Address-scoped (not connection-scoped) maintenance requests.
#if (defined(USE_ESP32) || defined(USE_RP2040_BLE)) && defined(USE_BLE_GATT_CLIENT)
conn_err_t unpair_device(uint64_t address);
#else
inline conn_err_t unpair_device(uint64_t) { return GATT_NOT_CONNECTED; }
#endif
#if defined(USE_ESP32) && defined(USE_BLE_GATT_CLIENT)
conn_err_t clear_gatt_cache(uint64_t address);
#else
inline conn_err_t clear_gatt_cache(uint64_t) { return GATT_NOT_CONNECTED; }
#endif

// send_service_ cursor states; >= 0 is the next service index to stream.
static constexpr int DONE_SENDING_SERVICES = -2;
static constexpr int INIT_SENDING_SERVICES = -3;
static constexpr int SERVICES_DONE_PENDING = -4;  // all batches delivered, done-message still owed
// Every sentinel must stay below the >= 0 streaming gate and clear of
// GATT_NOT_CONNECTED (-1) so cursor and error values can never be confused.
static_assert(DONE_SENDING_SERVICES < 0 && INIT_SENDING_SERVICES < 0 && SERVICES_DONE_PENDING < 0);
static_assert(DONE_SENDING_SERVICES != GATT_NOT_CONNECTED && INIT_SENDING_SERVICES != GATT_NOT_CONNECTED &&
              SERVICES_DONE_PENDING != GATT_NOT_CONNECTED);
// Owed-done retries stop here (~3 s at the 100 ms drain cadence): a done
// delivered near the client's 30 s timeout could land on a fresh request's
// empty accumulator and cache as an empty database.
static constexpr uint8_t SERVICES_DONE_RETRY_LIMIT = 30;
// Owed-ack retries stop after ~25 s of subscribed drain time from the first
// refusal, keeping most of the client's 30 s GATT window for congestion to
// clear while still bounding how stale a delivered reply can be.
static constexpr uint16_t PENDING_ACK_RETRY_LIMIT = 250;

// ---- Service-streaming size budget, shared by every platform's streamer ----

// Conservative MTU limit for API messages (accounts for WPA3 overhead)
static constexpr size_t MAX_PACKET_SIZE = 1360;

// Constants for size estimation
static constexpr uint8_t SERVICE_OVERHEAD_LEGACY = 25;     // UUID(20) + handle(4) + overhead(1)
static constexpr uint8_t SERVICE_OVERHEAD_EFFICIENT = 10;  // UUID(6) + handle(4)
static constexpr uint8_t CHAR_SIZE_128BIT = 35;            // UUID(20) + handle(4) + props(4) + overhead(7)
static constexpr uint8_t DESC_SIZE_128BIT = 25;            // UUID(20) + handle(4) + overhead(1)
static constexpr uint8_t DESC_PER_CHAR = 1;                // Assume 1 descriptor per characteristic

/// Estimate the wire size of a service (service overhead + its characteristics,
/// assuming 128-bit UUIDs and one 128-bit descriptor per characteristic to be
/// safe) before fetching/packing the full data.
inline size_t estimate_service_size(uint16_t char_count, bool use_efficient_uuids) {
  size_t service_overhead = use_efficient_uuids ? SERVICE_OVERHEAD_EFFICIENT : SERVICE_OVERHEAD_LEGACY;
  return service_overhead + (CHAR_SIZE_128BIT + DESC_SIZE_128BIT * DESC_PER_CHAR) * char_count;
}

// ---- UUID wire packing, shared by every platform's streamer ----

// This function is allocation-free and directly packs UUIDs into the output
// array using precalculated constants for the Bluetooth base UUID. ESPBTUUID
// stores its 128-bit form little-endian (same as Bluedroid).
inline void fill_128bit_uuid_array(std::array<uint64_t, 2> &out, const ble_device_base::ESPBTUUID &uuid) {
  using ble_device_base::ESPBTUUID;
  if (uuid.type() == ESPBTUUID::Type::UUID128) {
    const uint8_t *u = uuid.uuid128();
    // out[0] = bytes 8-15 (big-endian), out[1] = bytes 0-7 (big-endian)
    out[0] = ((uint64_t) u[15] << 56) | ((uint64_t) u[14] << 48) | ((uint64_t) u[13] << 40) | ((uint64_t) u[12] << 32) |
             ((uint64_t) u[11] << 24) | ((uint64_t) u[10] << 16) | ((uint64_t) u[9] << 8) | ((uint64_t) u[8]);
    out[1] = ((uint64_t) u[7] << 56) | ((uint64_t) u[6] << 48) | ((uint64_t) u[5] << 40) | ((uint64_t) u[4] << 32) |
             ((uint64_t) u[3] << 24) | ((uint64_t) u[2] << 16) | ((uint64_t) u[1] << 8) | ((uint64_t) u[0]);
    return;
  }
  // 16/32-bit UUID inserted into the Bluetooth base UUID:
  // 00000000-0000-1000-8000-00805F9B34FB
  uint32_t value = uuid.type() == ESPBTUUID::Type::UUID16 ? uuid.uuid16() : uuid.uuid32();
  out[0] = ((uint64_t) value << 32) | 0x00001000ULL;  // Base UUID bytes 8-11
  out[1] = 0x800000805F9B34FBULL;                     // Base UUID bytes 0-7
}

/// Fill the UUID in the appropriate wire format based on client support and
/// UUID type (128-bit array for old clients or 128-bit UUIDs, short form
/// otherwise).
inline void fill_gatt_uuid(std::array<uint64_t, 2> &uuid_128, uint32_t &short_uuid,
                           const ble_device_base::ESPBTUUID &uuid, bool use_efficient_uuids) {
  using ble_device_base::ESPBTUUID;
  if (!use_efficient_uuids || uuid.type() == ESPBTUUID::Type::UUID128) {
    fill_128bit_uuid_array(uuid_128, uuid);
  } else if (uuid.type() == ESPBTUUID::Type::UUID16) {
    short_uuid = uuid.uuid16();
  } else {
    short_uuid = uuid.uuid32();
  }
}

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
/// Result of close_service_batch: keep filling the batch or send it now.
/// An oversized service is packed alone; a failed (backpressured) send is
/// retried from the batch start, so no service is silently skipped.
enum class BatchClose : uint8_t { CONTINUE, SEND };

/// Close out the service just packed into resp (account its actual wire size,
/// advance the cursor) and decide whether the batch must be sent now. Shared
/// tail of both platform streamers so the budget logic and its log lines
/// cannot drift.
BatchClose close_service_batch(api::BluetoothGATTGetServicesResponse &resp, size_t &current_size, int16_t &send_service,
                               uint8_t connection_index, const char *address_str);
#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS

}  // namespace esphome::bluetooth_connection
