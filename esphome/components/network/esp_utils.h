#pragma once
#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && defined(USE_ESP32)
#include "esp_netif.h"
#include "esp_event.h"
namespace esphome {
namespace network {

/// Initialize ESP-IDF network interfaces and ensure the default event loop exists.
/// Returns true on success; logs and returns false on failure.
bool esp_init();

}  // namespace network
}  // namespace esphome
#endif
