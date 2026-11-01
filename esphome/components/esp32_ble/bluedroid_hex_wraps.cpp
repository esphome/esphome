// Linker wraps for the only two sscanf callers in Bluedroid, so newlib's scanf engine (~16 KB) is not linked.
// Same parsing as espressif/esp-idf#19235; drop once ESPHome uses an ESP-IDF release that contains it.

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_ESP32_BLE_BLUEDROID_HEX_WRAPS)

#include "bluedroid_hex.h"

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

#endif  // USE_ESP_IDF && USE_ESP32_BLE_BLUEDROID_HEX_WRAPS
