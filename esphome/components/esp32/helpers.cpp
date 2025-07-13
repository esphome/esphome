#include "esphome/core/helpers.h"

#ifdef USE_ESP32

#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_mac.h"

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include "esp_random.h"
#include "esp_system.h"

namespace esphome {

uint32_t random_uint32() { return esp_random(); }
bool random_bytes(uint8_t *data, size_t len) {
  esp_fill_random(data, len);
  return true;
}

Mutex::Mutex() { handle_ = xSemaphoreCreateMutex(); }
Mutex::~Mutex() {}
void Mutex::lock() { xSemaphoreTake(this->handle_, portMAX_DELAY); }
bool Mutex::try_lock() { return xSemaphoreTake(this->handle_, 0) == pdTRUE; }
void Mutex::unlock() { xSemaphoreGive(this->handle_); }

// only affects the executing core
// so should not be used as a mutex lock, only to get accurate timing
IRAM_ATTR InterruptLock::InterruptLock() { portDISABLE_INTERRUPTS(); }
IRAM_ATTR InterruptLock::~InterruptLock() { portENABLE_INTERRUPTS(); }

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
#if defined(CONFIG_SOC_IEEE802154_SUPPORTED)
  // When CONFIG_SOC_IEEE802154_SUPPORTED is defined, esp_efuse_mac_get_default
  // returns the 802.15.4 EUI-64 address, so we read directly from eFuse instead.
  if (has_custom_mac_address()) {
    esp_efuse_read_field_blob(ESP_EFUSE_MAC_CUSTOM, mac, 48);
  } else {
    esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, mac, 48);
  }
#else
  if (has_custom_mac_address()) {
    esp_efuse_mac_get_custom(mac);
  } else {
    esp_efuse_mac_get_default(mac);
  }
#endif
}

void set_mac_address(uint8_t *mac) { esp_base_mac_addr_set(mac); }

bool has_custom_mac_address() {
#if !defined(USE_ESP32_IGNORE_EFUSE_CUSTOM_MAC)
  uint8_t mac[6];
  // do not use 'esp_efuse_mac_get_custom(mac)' because it drops an error in the logs whenever it fails
#ifndef USE_ESP32_VARIANT_ESP32
  return (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_MAC_CUSTOM, mac, 48) == ESP_OK) && mac_address_is_valid(mac);
#else
  return (esp_efuse_read_field_blob(ESP_EFUSE_MAC_CUSTOM, mac, 48) == ESP_OK) && mac_address_is_valid(mac);
#endif
#else
  return false;
#endif
}

}  // namespace esphome

#endif  // USE_ESP32
