// Linker wraps for the only two sscanf callers in Bluedroid, so newlib's scanf engine (~16 KB) is not linked.
// Same parsing as espressif/esp-idf#19235; drop once ESPHome uses an ESP-IDF release that contains it.

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_ESP32_BLE_BLUEDROID_HEX_WRAPS)

#include "bluedroid_hex.h"

// Set with Bluedroid's private include dirs for this file only (native ESP-IDF toolchain)
#ifdef USE_ESP32_BLE_BLUEDROID_HEADERS
#include <type_traits>
extern "C" {
#include "btc/btc_config.h"
#include "device/bdaddr.h"
#include "stack/btm_api.h"
}
#endif

namespace esphome::esp32_ble {}

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {

bool btc_config_get_str(const char *section, const char *key, char *value, int *size_bytes);

bool __wrap_btc_config_get_bin(const char *section, const char *key, uint8_t *value, size_t *length) {
  return esphome::esp32_ble::config_get_bin(btc_config_get_str, section, key, value, length);
}

// bt_bdaddr_t is a struct holding uint8_t address[6]
bool __wrap_string_to_bdaddr(const char *string, uint8_t *addr) {
  return esphome::esp32_ble::parse_bdaddr(string, addr);
}

}  // extern "C"
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

#ifdef USE_ESP32_BLE_BLUEDROID_HEADERS
// ESP-IDF drift checks: if one fails, re-check the wraps against espressif/esp-idf#19235 and Bluedroid's sources.
static_assert(std::is_same_v<decltype(&btc_config_get_bin), decltype(&__wrap_btc_config_get_bin)>,
              "btc_config_get_bin signature changed");
static_assert(std::is_same_v<decltype(&btc_config_get_str), bool (*)(const char *, const char *, char *, int *)>,
              "btc_config_get_str signature changed");
static_assert(std::is_same_v<decltype(&string_to_bdaddr), bool (*)(const char *, bt_bdaddr_t *)>,
              "string_to_bdaddr signature changed");
static_assert(sizeof(bt_bdaddr_t) == 6 && sizeof(bt_bdaddr_t::address) == 6,
              "bt_bdaddr_t is no longer uint8_t address[6]");
static_assert(sizeof(tBTM_LE_KEY_VALUE) <= esphome::esp32_ble::CONFIG_BIN_STACK_BYTES,
              "Bluetooth LE keys outgrew the stack buffer of config_get_bin");
#endif

#endif  // USE_ESP_IDF && USE_ESP32_BLE_BLUEDROID_HEX_WRAPS
