// Counts notifications the GATT server's stack reported as not sent (ESP_GATTS_CONF_EVT with a bad status) while
// srvconf::active is set; gen_peri.py --burst wraps the register call (-Wl,--wrap=esp_ble_gatts_register_callback).
#pragma once
#include <esp_gatts_api.h>

namespace srvconf {
static esp_gatts_cb_t real_gatts = nullptr;
static volatile bool active = false;
static volatile uint32_t bad = 0;
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
  return __real_esp_ble_gatts_register_callback(srvconf::gatts_shim);
}
}
