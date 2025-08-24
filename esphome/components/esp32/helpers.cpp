#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

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

#ifdef CONFIG_LWIP_TCPIP_CORE_LOCKING
#include "lwip/priv/tcpip_priv.h"
#endif

LwIPLock::LwIPLock() {
#ifdef CONFIG_LWIP_TCPIP_CORE_LOCKING
  // When CONFIG_LWIP_TCPIP_CORE_LOCKING is enabled, lwIP uses a global mutex to protect
  // its internal state. Any thread can take this lock to safely access lwIP APIs.
  //
  // sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER) returns true if the current thread
  // already holds the lwIP core lock. This prevents recursive locking attempts and
  // allows nested LwIPLock instances to work correctly.
  //
  // If we don't already hold the lock, acquire it. This will block until the lock
  // is available if another thread currently holds it.
  if (!sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER)) {
    LOCK_TCPIP_CORE();
  }
#endif
}

LwIPLock::~LwIPLock() {
#ifdef CONFIG_LWIP_TCPIP_CORE_LOCKING
  // Only release the lwIP core lock if this thread currently holds it.
  //
  // sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER) queries lwIP's internal lock
  // ownership tracking. It returns true only if the current thread is registered
  // as the lock holder.
  //
  // This check is essential because:
  // 1. We may not have acquired the lock in the constructor (if we already held it)
  // 2. The lock might have been released by other means between constructor and destructor
  // 3. Calling UNLOCK_TCPIP_CORE() without holding the lock causes undefined behavior
  if (sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER)) {
    UNLOCK_TCPIP_CORE();
  }
#endif
}

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
