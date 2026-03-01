#pragma once

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include "espnow_compat.h"

namespace esphome::espnow {

static const espnow_err_t ESPNOW_ERR_BASE = 0x7000;
static const espnow_err_t ESPNOW_ERR_FAILED = (ESPNOW_ERR_BASE + 1);
static const espnow_err_t ESPNOW_ERR_OWN_ADDRESS = (ESPNOW_ERR_BASE + 2);
static const espnow_err_t ESPNOW_ERR_DATA_SIZE = (ESPNOW_ERR_BASE + 3);
static const espnow_err_t ESPNOW_ERR_PEER_NOT_SET = (ESPNOW_ERR_BASE + 4);
static const espnow_err_t ESPNOW_ERR_PEER_NOT_PAIRED = (ESPNOW_ERR_BASE + 5);
static const espnow_err_t ESPNOW_ERR_NOT_INIT = (ESPNOW_ERR_BASE + 6);
static const espnow_err_t ESPNOW_ERR_NO_MEM = (ESPNOW_ERR_BASE + 7);
static const espnow_err_t ESPNOW_ERR_INVALID_MAC = (ESPNOW_ERR_BASE + 8);

}  // namespace esphome::espnow

#endif  // USE_ESP32 || ESP8266
