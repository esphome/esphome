#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <esp_bt_defs.h>

namespace esphome {
namespace esp32_bt_classic {

const char *const TAG = "esp32_bt_classic";

// Helper for printing Bt MAC addresses for format "%02X:%02X:%02X:%02X:%02X:%02X"
#define EXPAND_MAC_F(addr) addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]

typedef enum {
  SCAN_STATUS_SCANNING = 0,
  SCAN_STATUS_FOUND,
  SCAN_STATUS_NOT_FOUND,
} scan_status_t;

const char *scan_status_to_str(scan_status_t status);
const char *esp_bt_status_to_str(esp_bt_status_t code);

// bd_addr_t <--> uint64_t conversion functions:
void uint64_to_bd_addr(uint64_t address, esp_bd_addr_t &bd_addr);
uint64_t bd_addr_to_uint64(const esp_bd_addr_t address);

// bd_addr_t <--> string conversion functions:
std::string bd_addr_to_str(const esp_bd_addr_t &addr);
bool str_to_bd_addr(const char *addr_str, esp_bd_addr_t &addr);

std::string u64_addr_to_str(uint64_t address);
uint64_t str_to_u64_addr(const char *addr_str);

template<typename T> void moveItemToBack(std::vector<T> &v, size_t item_index) {
  T tmp(v[item_index]);
  v.erase(v.begin() + item_index);
  v.push_back(tmp);
}

}  // namespace esp32_bt_classic
}  // namespace esphome

#endif  // USE_ESP32
