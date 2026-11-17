#pragma once
// Feeds fabricated conn param update and disconnect events into ESP32BLE's static handlers.
// The event queue takes a single writer: call run() only while scanning is stopped and no connection is active.
#include <cstring>
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/core/log.h"

namespace cpinject {
using GapFn = void (*)(esp_gap_ble_cb_event_t, esp_ble_gap_cb_param_t *);
using GattcFn = void (*)(esp_gattc_cb_event_t, esp_gatt_if_t, esp_ble_gattc_cb_param_t *);
GapFn steal_gap();
GattcFn steal_gattc();
// Explicit instantiation ignores access checks, which exposes the protected static handlers.
template<GapFn F> struct GapThief {
  friend GapFn steal_gap() { return F; }
};
template<GattcFn F> struct GattcThief {
  friend GattcFn steal_gattc() { return F; }
};
template struct GapThief<&esphome::esp32_ble::ESP32BLE::gap_event_handler>;
template struct GattcThief<&esphome::esp32_ble::ESP32BLE::gattc_event_handler>;

static const char *const TAG = "cpinject";

inline void peer(uint8_t n, uint8_t *bda) {
  const uint8_t base[6] = {0xC0, 0xFF, 0xEE, 0x00, 0x00, 0x00};
  memcpy(bda, base, 6);
  bda[5] = n;
}

inline void update(uint8_t n, int status, int times) {
  for (int i = 0; i < times; i++) {
    esp_ble_gap_cb_param_t p{};
    peer(n, p.update_conn_params.bda);
    p.update_conn_params.status = (esp_bt_status_t) status;
    p.update_conn_params.conn_int = 24;
    p.update_conn_params.timeout = 400;
    steal_gap()(ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT, &p);
  }
}

inline void disconnect(uint8_t n) {
  esp_ble_gattc_cb_param_t p{};
  peer(n, p.disconnect.remote_bda);
  p.disconnect.reason = ESP_GATT_CONN_TERMINATE_PEER_USER;
  p.disconnect.conn_id = 0xEE;
  // 0xFE matches no registered client, so the queued event is ignored by every connection.
  steal_gattc()(ESP_GATTC_DISCONNECT_EVT, 0xFE, &p);
}

inline void run() {
  ESP_LOGI(TAG, "STEP1 peers 1-4 fail 5x each (3 slots)");
  for (uint8_t n = 1; n <= 4; n++)
    update(n, 6, 5);
  ESP_LOGI(TAG, "STEP2 peer 1 succeeds, then fails 3x");
  update(1, 0, 1);
  update(1, 6, 3);
  ESP_LOGI(TAG, "STEP3 peer 2 disconnects, then fails 3x");
  disconnect(2);
  update(2, 6, 3);
  ESP_LOGI(TAG, "STEP4 peer 4 fails 3x, slots still full");
  update(4, 6, 3);
  ESP_LOGI(TAG, "STEP5 peer 3 disconnects, peer 4 fails 3x");
  disconnect(3);
  update(4, 6, 3);
  ESP_LOGI(TAG, "STEP6 done");
}
}  // namespace cpinject
