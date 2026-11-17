#pragma once
#include <cstring>
#include <esp_gap_ble_api.h>
#include "esphome/core/log.h"
#include "central_bda.hpp"  // written by run.sh from PROXY_BT_MAC

// 50 ms interval, peripheral latency 10, 6 s supervision timeout
inline void btpeer_request_latency() {
  esp_ble_conn_update_params_t p{};
  memcpy(p.bda, CENTRAL_BDA, 6);
  p.min_int = 40;
  p.max_int = 40;
  p.latency = 10;
  p.timeout = 600;
  esp_err_t err = esp_ble_gap_update_conn_params(&p);
  ESP_LOGI("peer", "requested interval 50 ms latency 10 timeout 6 s: %d", err);
}
