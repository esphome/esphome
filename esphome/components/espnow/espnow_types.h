#pragma once

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include <cstddef>
#include <cstdint>

#if defined(USE_ESP8266)

extern "C" {
#include <user_interface.h>
#include <espnow.h>
}

#ifndef ESP_NOW_ETH_ALEN
static constexpr uint8_t ESP_NOW_ETH_ALEN = 6;
#endif

#ifndef ESP_NOW_MAX_DATA_LEN
static constexpr uint8_t ESP_NOW_MAX_DATA_LEN = 250;
#endif

#ifndef ESP_NOW_SEND_SUCCESS
static constexpr uint8_t ESP_NOW_SEND_SUCCESS = 0;
#endif

#ifndef ESP_NOW_SEND_FAIL
static constexpr uint8_t ESP_NOW_SEND_FAIL = 1;
#endif

#ifndef ESP_ERR_ESPNOW_BASE
static constexpr int ESP_ERR_ESPNOW_BASE = 0x3064;
#endif
#ifndef ESP_ERR_ESPNOW_ARG
static constexpr int ESP_ERR_ESPNOW_ARG = ESP_ERR_ESPNOW_BASE + 2;
#endif
#ifndef ESP_ERR_ESPNOW_INTERNAL
static constexpr int ESP_ERR_ESPNOW_INTERNAL = ESP_ERR_ESPNOW_BASE + 6;
#endif
#ifndef ESP_ERR_ESPNOW_NOT_FOUND
static constexpr int ESP_ERR_ESPNOW_NOT_FOUND = ESP_ERR_ESPNOW_BASE + 5;
#endif
#ifndef ESP_ERR_ESPNOW_IF
static constexpr int ESP_ERR_ESPNOW_IF = ESP_ERR_ESPNOW_BASE + 8;
#endif

namespace esphome::espnow {

using espnow_err_t = int;
using espnow_send_status_t = uint8_t;

static constexpr espnow_err_t ESPNOW_OK = 0;
static constexpr size_t ESPNOW_ETH_ALEN = ESP_NOW_ETH_ALEN;
static constexpr size_t ESPNOW_MAX_DATA_LEN = ESP_NOW_MAX_DATA_LEN;
static constexpr espnow_send_status_t ESPNOW_SEND_SUCCESS = ESP_NOW_SEND_SUCCESS;
static constexpr espnow_send_status_t ESPNOW_SEND_FAIL = ESP_NOW_SEND_FAIL;

inline bool espnow_send_status_success(espnow_send_status_t status) { return status == ESP_NOW_SEND_SUCCESS; }
inline bool espnow_is_peer_exist(const uint8_t *mac_addr) {
  return esp_now_is_peer_exist(const_cast<uint8_t *>(mac_addr)) == ESPNOW_OK;
}

}  // namespace esphome::espnow

#else

#include <esp_err.h>
#include <esp_now.h>

namespace esphome::espnow {

using espnow_err_t = esp_err_t;
using espnow_send_status_t = esp_now_send_status_t;

static constexpr espnow_err_t ESPNOW_OK = ESP_OK;
static constexpr size_t ESPNOW_ETH_ALEN = ESP_NOW_ETH_ALEN;
static constexpr size_t ESPNOW_MAX_DATA_LEN = ESP_NOW_MAX_DATA_LEN;

inline bool espnow_send_status_success(espnow_send_status_t status) { return status == ESP_NOW_SEND_SUCCESS; }
inline bool espnow_is_peer_exist(const uint8_t *mac_addr) { return esp_now_is_peer_exist(mac_addr); }

}  // namespace esphome::espnow

#endif

#endif  // USE_ESP32 || USE_ESP8266
