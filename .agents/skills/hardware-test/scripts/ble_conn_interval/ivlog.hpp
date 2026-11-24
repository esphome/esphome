// Logs the connection interval of every new GATTC link and every conn param update (wraps the two register calls).
#pragma once
#include <esp_gattc_api.h>
#include <esp_gap_ble_api.h>
#include <esp_bt.h>
#include "esphome/core/log.h"

namespace ivlog {
static esp_gattc_cb_t real_gattc = nullptr;
static esp_gap_ble_cb_t real_gap = nullptr;
static void gattc_shim(esp_gattc_cb_event_t e, esp_gatt_if_t i, esp_ble_gattc_cb_param_t *p) {
  if (e == ESP_GATTC_CONNECT_EVT) {
    ESP_LOGI("ivlog", "IVLOG connect %02X:%02X if=%d interval=%u latency=%u timeout=%u", p->connect.remote_bda[4],
             p->connect.remote_bda[5], i, p->connect.conn_params.interval, p->connect.conn_params.latency,
             p->connect.conn_params.timeout);
  }
  real_gattc(e, i, p);
}
static void gap_shim(esp_gap_ble_cb_event_t e, esp_ble_gap_cb_param_t *p) {
  if (e == ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT) {
    ESP_LOGI("ivlog", "IVLOG update %02X:%02X status=%d interval=%u min=%u max=%u", p->update_conn_params.bda[4],
             p->update_conn_params.bda[5], p->update_conn_params.status, p->update_conn_params.conn_int,
             p->update_conn_params.min_int, p->update_conn_params.max_int);
  }
  real_gap(e, p);
}
}  // namespace ivlog

extern "C" {
esp_err_t __real_esp_ble_gattc_register_callback(esp_gattc_cb_t cb);
esp_err_t __real_esp_ble_gap_register_callback(esp_gap_ble_cb_t cb);
esp_err_t __wrap_esp_ble_gattc_register_callback(esp_gattc_cb_t cb) {
  ivlog::real_gattc = cb;
  return __real_esp_ble_gattc_register_callback(ivlog::gattc_shim);
}
esp_err_t __wrap_esp_ble_gap_register_callback(esp_gap_ble_cb_t cb) {
  ivlog::real_gap = cb;
  return __real_esp_ble_gap_register_callback(ivlog::gap_shim);
}
}
