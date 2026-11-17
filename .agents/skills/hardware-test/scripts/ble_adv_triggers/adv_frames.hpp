#pragma once
// Rotating non-connectable advertisements with known service and manufacturer data (random static addresses).
#include <esp_gap_ble_api.h>
#include <cstring>
#include "esphome/core/log.h"

namespace af {
struct Frame {
  uint8_t last;  // address C4:C1:38:00:00:<last>
  uint8_t len;
  uint8_t adv[31];
};
// AD structures after the flags. 0x16 = service data 16-bit, 0x20 = 32-bit, 0x21 = 128-bit, 0xFF = manufacturer.
static const Frame FRAMES[] = {
    // 21: service 181C AA01 + manufacturer 02E5 BB01
    {0x21, 12, {5, 0x16, 0x1C, 0x18, 0xAA, 0x01, 5, 0xFF, 0xE5, 0x02, 0xBB, 0x01}},
    // 22: service 32-bit 12345678 AA02
    {0x22, 8, {7, 0x20, 0x78, 0x56, 0x34, 0x12, 0xAA, 0x02}},
    // 23: service 128-bit 12345678-9ABC-DEF0-1122-334455667788 (wire order) AA03
    {0x23, 20, {19,   0x21, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
                0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, 0xAA, 0x03}},
    // 24: manufacturer 181C BB04 (same id as the service UUID of 21, other data kind)
    {0x24, 6, {5, 0xFF, 0x1C, 0x18, 0xBB, 0x04}},
    // 25: service 181C AA05 then a second 181C entry AA06 (only the first may fire)
    {0x25, 12, {5, 0x16, 0x1C, 0x18, 0xAA, 0x05, 5, 0x16, 0x1C, 0x18, 0xAA, 0x06}},
    // 26: the 128-bit UUID in printed byte order AA07 (must not match)
    {0x26, 20, {19,   0x21, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0xAA, 0x07}},
};
static constexpr int COUNT = sizeof(FRAMES) / sizeof(FRAMES[0]);
static uint32_t failed = 0;
static uint32_t sent = 0;

inline void send(int i) {
  const Frame &f = FRAMES[i];
  uint8_t adv[31];
  uint8_t n = 0;
  adv[n++] = 2;
  adv[n++] = 0x01;
  adv[n++] = 0x06;
  memcpy(adv + n, f.adv, f.len);
  n += f.len;
  esp_ble_gap_stop_advertising();
  uint8_t addr[6] = {0xC4, 0xC1, 0x38, 0x00, 0x00, f.last};
  esp_err_t e1 = esp_ble_gap_set_rand_addr(addr);
  esp_err_t e2 = esp_ble_gap_config_adv_data_raw(adv, n);
  esp_ble_adv_params_t p{};
  p.adv_int_min = 0x20;  // 20 ms
  p.adv_int_max = 0x20;
  p.adv_type = ADV_TYPE_NONCONN_IND;
  p.own_addr_type = BLE_ADDR_TYPE_RANDOM;
  p.channel_map = ADV_CHNL_ALL;
  p.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  esp_err_t e3 = esp_ble_gap_start_advertising(&p);
  sent++;
  if (e1 != ESP_OK || e2 != ESP_OK || e3 != ESP_OK) {
    failed++;
    ESP_LOGW("af", "frame %d failed: %d %d %d", i, e1, e2, e3);
  }
}
}  // namespace af
