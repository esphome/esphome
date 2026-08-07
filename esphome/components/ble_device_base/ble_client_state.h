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
