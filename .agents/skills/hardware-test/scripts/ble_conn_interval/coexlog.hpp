// Logs every esp_coex_status_bit_set/clear with the BLE status the coexistence scheduler reads back (-Wl,--wrap).
#pragma once
#include <esp_coexist.h>
#include "esphome/core/log.h"

extern "C" {
uint32_t coex_schm_status_get(uint32_t type);  // libcoexist
esp_err_t __real_esp_coex_status_bit_set(esp_coex_status_type_t type, uint32_t status);
esp_err_t __real_esp_coex_status_bit_clear(esp_coex_status_type_t type, uint32_t status);
esp_err_t __wrap_esp_coex_status_bit_set(esp_coex_status_type_t type, uint32_t status) {
  esp_err_t err = __real_esp_coex_status_bit_set(type, status);
  ESP_LOGI("coexlog", "COEXBIT set type=%d bits=0x%x err=%d ble_status=0x%x", (int) type, (unsigned) status, err,
           (unsigned) coex_schm_status_get(1));
  return err;
}
esp_err_t __wrap_esp_coex_status_bit_clear(esp_coex_status_type_t type, uint32_t status) {
  esp_err_t err = __real_esp_coex_status_bit_clear(type, status);
  ESP_LOGI("coexlog", "COEXBIT clear type=%d bits=0x%x err=%d ble_status=0x%x", (int) type, (unsigned) status, err,
           (unsigned) coex_schm_status_get(1));
  return err;
}
}
