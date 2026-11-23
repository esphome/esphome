// Counts notifications the GATT server could not send while srvconf::active is set: refused at once by
// esp_ble_gatts_send_indicate, or reported late (ESP_GATTS_CONF_EVT with a bad status). gen_peri.py --burst wraps
// both calls (-Wl,--wrap=esp_ble_gatts_register_callback, -Wl,--wrap=esp_ble_gatts_send_indicate).
#pragma once
#include <atomic>
#include <esp_gatts_api.h>

namespace srvconf {
static esp_gatts_cb_t real_gatts = nullptr;
// the Bluetooth task writes these while the main loop resets and reads them
static std::atomic<bool> active{false};
static std::atomic<uint32_t> bad{0};
static std::atomic<uint32_t> refused{0};
static bool installed = false;     // the register wrap ran: without it late failures are not counted at all
static bool send_wrapped = false;  // the send wrap ran: without it refused sends are not counted
inline bool complete() { return installed && send_wrapped; }
static void gatts_shim(esp_gatts_cb_event_t e, esp_gatt_if_t i, esp_ble_gatts_cb_param_t *p) {
  if (e == ESP_GATTS_CONF_EVT && active && p->conf.status != ESP_GATT_OK)
    bad++;
  real_gatts(e, i, p);
}
}  // namespace srvconf

extern "C" {
esp_err_t __real_esp_ble_gatts_register_callback(esp_gatts_cb_t cb);
esp_err_t __wrap_esp_ble_gatts_register_callback(esp_gatts_cb_t cb) {
  srvconf::real_gatts = cb;
  srvconf::installed = true;
  return __real_esp_ble_gatts_register_callback(srvconf::gatts_shim);
}
esp_err_t __real_esp_ble_gatts_send_indicate(esp_gatt_if_t gatts_if, uint16_t conn_id, uint16_t attr_handle,
                                             uint16_t value_len, uint8_t *value, bool need_confirm);
esp_err_t __wrap_esp_ble_gatts_send_indicate(esp_gatt_if_t gatts_if, uint16_t conn_id, uint16_t attr_handle,
                                             uint16_t value_len, uint8_t *value, bool need_confirm) {
  srvconf::send_wrapped = true;
  esp_err_t err = __real_esp_ble_gatts_send_indicate(gatts_if, conn_id, attr_handle, value_len, value, need_confirm);
  if (err != ESP_OK && srvconf::active)
    srvconf::refused++;
  return err;
}
}
