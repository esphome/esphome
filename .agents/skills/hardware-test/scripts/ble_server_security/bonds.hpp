#pragma once
#include <esp_err.h>
#include <esp_gap_ble_api.h>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
// Logs the bond list Bluedroid loaded from NVS
inline void log_bonds(const char *who) {
  int n = esp_ble_get_bond_device_num();
  esp_ble_bond_dev_t list[8];
  int len = n > 8 ? 8 : n;
  if (len > 0) {
    esp_err_t err = esp_ble_get_bond_device_list(&len, list);
    if (err != ESP_OK) {
      ESP_LOGW("bonds", "%s: %d bonded, bond list failed: %s", who, n, esp_err_to_name(err));
      return;
    }
  }
  ESP_LOGI("bonds", "%s: %d bonded", who, n);
  for (int i = 0; i < len; i++) {
    char mac[esphome::MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    const uint8_t *k = list[i].bond_key.penc_key.ltk;
    ESP_LOGI("bonds", "%s: bond %d %s key_mask=%02x ltk=%02x%02x..%02x%02x", who, i,
             esphome::format_mac_addr_upper(list[i].bd_addr, mac), list[i].bond_key.key_mask, k[0], k[1], k[14], k[15]);
  }
}
