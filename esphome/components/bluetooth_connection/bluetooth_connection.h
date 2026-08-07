// Shared types for the per-platform GATT connection backends and the
// Bluetooth proxy that drives them.

#pragma once

#include "esphome/core/defines.h"

#include "esphome/components/ble_device_base/ble_client_state.h"

#ifdef USE_ESP32
#include <esp_err.h>
#endif

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

// send_service_ cursor states; >= 0 is the next service index to stream.
static constexpr int DONE_SENDING_SERVICES = -2;
static constexpr int INIT_SENDING_SERVICES = -3;

}  // namespace esphome::bluetooth_connection
