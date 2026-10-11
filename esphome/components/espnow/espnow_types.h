#pragma once

#include <cstddef>
#include <cstdint>

#if defined(USE_ESP32)

#include <esp_err.h>
#include <esp_now.h>

#elif defined(USE_ESP8266)

extern "C" {
#include <c_types.h>
#include <espnow.h>
#include <user_interface.h>
}

#endif

namespace esphome::espnow {

struct WifiPacketRxControl {
  int8_t rssi;         // Received Signal Strength Indicator (RSSI) of packet, unit: dBm
  uint32_t timestamp;  // Timestamp in microseconds when the packet was received, precise only if modem sleep or
                       // light sleep is not enabled
};

#if defined(USE_ESP32)

// Handlers read rx_ctrl through the ESP-IDF type so existing lambdas keep compiling
using rx_ctrl_t = wifi_pkt_rx_ctrl_t;
static constexpr bool ESPNOW_REPORTS_DESTINATION = true;

// Maximum size of the ESPNow event queues - must be a power of 2 for the lock-free queue
static constexpr size_t MAX_ESP_NOW_SEND_QUEUE_SIZE = 16;
static constexpr size_t MAX_ESP_NOW_RECEIVE_QUEUE_SIZE = 16;

#elif defined(USE_ESP8266)

// The NONOS SDK has no esp_err_t vocabulary. These are the ESP-IDF names and values the shared espnow code
// uses, so it reads the same on both platforms.
using esp_err_t = int32_t;

static constexpr esp_err_t ESP_OK = 0;
static constexpr esp_err_t ESP_FAIL = -1;
static constexpr esp_err_t ESP_ERR_INVALID_MAC = 0x10B;
static constexpr esp_err_t ESP_ERR_ESPNOW_BASE = 0x3064;
static constexpr esp_err_t ESP_ERR_ESPNOW_NOT_INIT = ESP_ERR_ESPNOW_BASE + 1;
static constexpr esp_err_t ESP_ERR_ESPNOW_ARG = ESP_ERR_ESPNOW_BASE + 2;
static constexpr esp_err_t ESP_ERR_ESPNOW_NO_MEM = ESP_ERR_ESPNOW_BASE + 3;
static constexpr esp_err_t ESP_ERR_ESPNOW_NOT_FOUND = ESP_ERR_ESPNOW_BASE + 5;
static constexpr esp_err_t ESP_ERR_ESPNOW_INTERNAL = ESP_ERR_ESPNOW_BASE + 6;
static constexpr esp_err_t ESP_ERR_ESPNOW_IF = ESP_ERR_ESPNOW_BASE + 8;

inline const char *esp_err_to_name(esp_err_t err) { return err == ESP_OK ? "ESP_OK" : "ESP_FAIL"; }

static constexpr uint8_t ESP_NOW_ETH_ALEN = 6;
static constexpr uint8_t ESP_NOW_MAX_DATA_LEN = 250;

// NOLINTNEXTLINE(readability-identifier-naming)
enum esp_now_send_status_t : uint8_t {
  ESP_NOW_SEND_SUCCESS = 0,
  ESP_NOW_SEND_FAIL = 1,
};

using rx_ctrl_t = WifiPacketRxControl;
// The SDK reports neither the destination address nor the signal strength of a received frame
static constexpr bool ESPNOW_REPORTS_DESTINATION = false;

// Half the ESP32 depth: the heap is far smaller and only one send is ever in flight
static constexpr size_t MAX_ESP_NOW_SEND_QUEUE_SIZE = 8;
static constexpr size_t MAX_ESP_NOW_RECEIVE_QUEUE_SIZE = 8;

#endif

}  // namespace esphome::espnow
