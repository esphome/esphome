#include "socket_ble.h"

#ifdef USE_ZEPHYR
#include <cstdio>
#include "esphome/core/log.h"

namespace esphome::socket_ble {

static const char *const TAG = "socket_ble";

size_t format_bdaddr_to(const bdaddr_t addr, std::span<char, BDADDR_STR_LEN> buf) {
  // The address is stored least significant byte first and printed most significant byte first
  int written = std::snprintf(buf.data(), buf.size(), "%02X:%02X:%02X:%02X:%02X:%02X", addr[5], addr[4], addr[3],
                              addr[2], addr[1], addr[0]);
  if (written < 0 || static_cast<size_t>(written) >= buf.size()) {
    ESP_LOGE(TAG, "Failed to format Bluetooth address");
    return 0;
  }
  return static_cast<size_t>(written);
}

}  // namespace esphome::socket_ble

#endif  // USE_ZEPHYR
