#pragma once
#include <esp_gap_ble_api.h>
#include "esphome/core/log.h"
// Logs the bond list Bluedroid loaded from NVS
inline void log_bonds(const char *who) {
  int n = esp_ble_get_bond_device_num();
  esp_ble_bond_dev_t list[8];
  int len = n > 8 ? 8 : n;
  if (len > 0)
    esp_ble_get_bond_device_list(&len, list);
  ESP_LOGI("bonds", "%s: %d bonded", who, n);
  for (int i = 0; i < len; i++) {
    const uint8_t *a = list[i].bd_addr;
    const uint8_t *k = list[i].bond_key.penc_key.ltk;
    ESP_LOGI("bonds", "%s: bond %d %02X:%02X:%02X:%02X:%02X:%02X key_mask=%02x ltk=%02x%02x..%02x%02x", who, i, a[0],
             a[1], a[2], a[3], a[4], a[5], list[i].bond_key.key_mask, k[0], k[1], k[14], k[15]);
  }
}
