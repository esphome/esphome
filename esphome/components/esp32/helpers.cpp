#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_mac.h"

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include "esphome/core/log.h"
#include "esp_random.h"
#include "esp_system.h"

namespace esphome {

ESPHOME_LOG_TAG(TAG, "esp32");

bool random_bytes(uint8_t *data, size_t len) {
  esp_fill_random(data, len);
  return true;
}

// only affects the executing core
// so should not be used as a mutex lock, only to get accurate timing
IRAM_ATTR InterruptLock::InterruptLock() { portDISABLE_INTERRUPTS(); }
IRAM_ATTR InterruptLock::~InterruptLock() { portENABLE_INTERRUPTS(); }

#ifdef CONFIG_LWIP_TCPIP_CORE_LOCKING
#include "lwip/priv/tcpip_priv.h"
#endif

LwIPLock::LwIPLock() {
#ifdef CONFIG_LWIP_TCPIP_CORE_LOCKING
  // A thread that already holds the core lock (lwIP took it for a tcpip_api_call or netif callback, or an
  // outer LwIPLock did) must not take it again, and must not release it when this nested guard ends.
  if (!sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER)) {
    LOCK_TCPIP_CORE();
    this->locked_ = true;
  }
#endif
}

LwIPLock::~LwIPLock() {
#ifdef CONFIG_LWIP_TCPIP_CORE_LOCKING
  if (this->locked_) {
    UNLOCK_TCPIP_CORE();
  }
#endif
}

/// Read MAC and validate both the return code and content.
static bool read_valid_mac(uint8_t *mac, esp_err_t err) { return err == ESP_OK && mac_address_is_valid(mac); }

static constexpr size_t MAC_ADDRESS_SIZE_BITS = MAC_ADDRESS_SIZE * 8;  // 48 bits

// Must not use the ESPHome logger (may run before it exists, e.g. from app_main()).
bool get_custom_mac_address(uint8_t *mac) {
  // has_custom_mac_address() checks the raw eFuse field, while the reads below select their
  // method differently and may still fail (CRC), so the result must be validated again.
  if (!has_custom_mac_address())
    return false;
#if defined(CONFIG_SOC_IEEE802154_SUPPORTED)
  return read_valid_mac(mac, esp_efuse_read_field_blob(ESP_EFUSE_MAC_CUSTOM, mac, MAC_ADDRESS_SIZE_BITS));
#else
  return read_valid_mac(mac, esp_efuse_mac_get_custom(mac));
#endif
}

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
  if (get_custom_mac_address(mac)) {
    return;
  }
#if defined(CONFIG_SOC_IEEE802154_SUPPORTED)
  // When CONFIG_SOC_IEEE802154_SUPPORTED is defined, esp_efuse_mac_get_default
  // returns the 802.15.4 EUI-64 address, so we read directly from eFuse instead.
  // This already reads raw eFuse bytes, so there is no CRC-bypass fallback
  // (unlike the non-IEEE802154 path where esp_efuse_mac_get_default does CRC checks).
  if (read_valid_mac(mac, esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, mac, MAC_ADDRESS_SIZE_BITS))) {
    return;
  }
#else
  if (read_valid_mac(mac, esp_efuse_mac_get_default(mac))) {
    return;
  }
  // Default MAC read failed (e.g., eFuse CRC error) - try reading raw eFuse bytes
  // directly, bypassing CRC validation. A MAC that passes mac_address_is_valid()
  // (non-zero, non-broadcast, unicast) is almost certainly the real factory MAC
  // with a corrupted CRC byte, which is far better than returning garbage or zeros.
  if (read_valid_mac(mac, esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, mac, MAC_ADDRESS_SIZE_BITS))) {
    ESP_LOGW(TAG, "eFuse MAC CRC failed but raw bytes appear valid - using raw eFuse MAC");
    return;
  }
#endif
  // All methods failed - zero the MAC rather than returning garbage
  ESP_LOGE(TAG, "Failed to read a valid MAC address from eFuse");
  memset(mac, 0, MAC_ADDRESS_SIZE);
}

void set_mac_address(uint8_t *mac) { esp_base_mac_addr_set(mac); }

bool has_custom_mac_address() {
#if !defined(USE_ESP32_IGNORE_EFUSE_CUSTOM_MAC)
  uint8_t mac[MAC_ADDRESS_SIZE];
  // do not use 'esp_efuse_mac_get_custom(mac)' because it drops an error in the logs whenever it fails
#ifndef USE_ESP32_VARIANT_ESP32
  return (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_MAC_CUSTOM, mac, MAC_ADDRESS_SIZE_BITS) == ESP_OK) &&
         mac_address_is_valid(mac);
#else
  return (esp_efuse_read_field_blob(ESP_EFUSE_MAC_CUSTOM, mac, MAC_ADDRESS_SIZE_BITS) == ESP_OK) &&
         mac_address_is_valid(mac);
#endif
#else
  return false;
#endif
}

}  // namespace esphome

#endif  // USE_ESP32
