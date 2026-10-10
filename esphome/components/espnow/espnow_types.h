#pragma once

#include <cstdint>

#if defined(USE_ESP32)

#include <esp_err.h>
#include <esp_now.h>

#elif defined(USE_ESP8266)

extern "C" {
#include <espnow.h>
#include <user_interface.h>
}

namespace esphome::espnow {

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
static constexpr esp_err_t ESP_ERR_ESPNOW_FULL = ESP_ERR_ESPNOW_BASE + 4;
static constexpr esp_err_t ESP_ERR_ESPNOW_NOT_FOUND = ESP_ERR_ESPNOW_BASE + 5;
static constexpr esp_err_t ESP_ERR_ESPNOW_INTERNAL = ESP_ERR_ESPNOW_BASE + 6;
static constexpr esp_err_t ESP_ERR_ESPNOW_EXIST = ESP_ERR_ESPNOW_BASE + 7;
static constexpr esp_err_t ESP_ERR_ESPNOW_IF = ESP_ERR_ESPNOW_BASE + 8;

static constexpr uint8_t ESP_NOW_ETH_ALEN = 6;
static constexpr uint8_t ESP_NOW_MAX_DATA_LEN = 250;

// NOLINTNEXTLINE(readability-identifier-naming)
enum esp_now_send_status_t : uint8_t {
  ESP_NOW_SEND_SUCCESS = 0,
  ESP_NOW_SEND_FAIL = 1,
};

}  // namespace esphome::espnow

#endif
