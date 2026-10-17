// ble_client_state.h
//
// Platform-neutral GATT client connection state types, shared by every
// platform's GATT client implementation (esp32_ble_client, bluetooth_connection
// backends). Moved here from esp32_ble_tracker, which re-exports them under its
// own namespace for backward compatibility.

#pragma once

#include <cstdint>

namespace esphome::ble_device_base {

/// ESPHome-private errors for the API's plain-int error fields, outside the
/// ATT code range so they cannot be mistaken for spec errors. -1 is
/// understood by API clients as "not connected". Shared by every GATT
/// client backend.
static constexpr int GATT_ERR_NOT_CONNECTED = -1;
static constexpr int GATT_ERR_NO_MEMORY = -2;
/// ATT "Unlikely Error" (spec 0x0E): a client-side internal inconsistency,
/// e.g. a service table failing its own bounds checks.
static constexpr int GATT_ERR_UNLIKELY = 0x0E;

/// Safety net shared by every GATT backend: force IDLE when the stack never
/// delivers its disconnect completion.
static constexpr uint32_t GATT_DISCONNECT_TIMEOUT_MS = 10000;

/// ATT MTU before negotiation completes (Bluetooth spec default).
static constexpr uint16_t DEFAULT_ATT_MTU = 23;

// Preferred connection parameters shared by every platform's GATT client so
// the backends cannot drift (units: interval 1.25 ms, timeout 10 ms; latency
// 0). FAST covers connection setup and service discovery; MEDIUM is the
// steady state once established. Stack defaults (12.5-15 ms) are too slow for
// stable connections through WiFi-based BLE proxies, causing disconnections;
// MEDIUM balances responsiveness with bandwidth usage.
static constexpr uint16_t MEDIUM_MIN_CONN_INTERVAL = 0x07;  // 7 * 1.25ms = 8.75ms
static constexpr uint16_t MEDIUM_MAX_CONN_INTERVAL = 0x09;  // 9 * 1.25ms = 11.25ms
// The timeout value was increased from 6s to 8s to address stability issues observed
// in certain BLE devices when operating through WiFi-based BLE proxies. The longer
// timeout reduces the likelihood of disconnections during periods of high latency.
static constexpr uint16_t MEDIUM_CONN_TIMEOUT = 800;  // 800 * 10ms = 8s

// Fastest connection parameters for devices with short discovery timeouts
static constexpr uint16_t FAST_MIN_CONN_INTERVAL = 0x06;  // 6 * 1.25ms = 7.5ms (BLE minimum)
static constexpr uint16_t FAST_MAX_CONN_INTERVAL = 0x06;  // 6 * 1.25ms = 7.5ms
static constexpr uint16_t FAST_CONN_TIMEOUT = 1000;       // 1000 * 10ms = 10s

enum class ClientState : uint8_t {
  // Connection is allocated
  INIT,
  // Client is disconnecting
  DISCONNECTING,
  // Connection is idle, no device detected.
  IDLE,
  // Device advertisement found.
  DISCOVERED,
  // Connection in progress.
  CONNECTING,
  // Initial connection established.
  CONNECTED,
  // The client and sub-clients have completed setup.
  ESTABLISHED,
};

// Helper function to convert ClientState to string
const char *client_state_to_string(ClientState state);

enum class ConnectionType : uint8_t {
  // The default connection type, we hold all the services in ram
  // for the duration of the connection.
  V1,
  // The client has a cache of the services and mtu so we should not
  // fetch them again
  V3_WITH_CACHE,
  // The client does not need the services and mtu once we send them
  // so we should wipe them from memory as soon as we send them
  V3_WITHOUT_CACHE
};

}  // namespace esphome::ble_device_base
