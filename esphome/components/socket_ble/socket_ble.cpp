#include "headers.h"
#include "socket_ble.h"
#include <string>
#include <span>
#include "esphome/core/log.h"

namespace esphome::socket_ble {

static const char *const TAG = "socket_ble";

// Format sockaddr into caller-provided buffer, returns length written (excluding null)
size_t format_bdaddr_to(const bdaddr_t addr, std::span<char, BDADDR_STR_LEN> buf) {
  // Format: "XX:XX:XX:XX:XX:XX" = 17 chars + null = 18
  int written = std::snprintf(buf.data(), buf.size(), "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2],
                              addr[3], addr[4], addr[5]);
  if (written < 0 || static_cast<size_t>(written) >= buf.size()) {
    ESP_LOGE(TAG, "Failed to format Bluetooth address");
    return 0;
  }
  return static_cast<size_t>(written);
}

}  // namespace esphome::socket_ble
