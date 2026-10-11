#include "lt_component.h"

#ifdef USE_LIBRETINY

#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::libretiny {

ESPHOME_LOG_TAG(TAG, "libretiny");

void LTComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LibreTiny:\n"
                "  Version: %s\n"
                "  Loglevel: %u",
                &LT_BANNER_STR[10], LT_LOGLEVEL);
#if defined(__OPTIMIZE_SIZE__) && __OPTIMIZE_LEVEL__ > 0 && __OPTIMIZE_LEVEL__ <= 3
  ESP_LOGCONFIG(TAG, "  Optimization: -Os, SDK: -O" STRINGIFY_MACRO(__OPTIMIZE_LEVEL__));
#endif

#if defined(USE_LN882X) && defined(USE_WIFI)
  // The SDK falls back to this MAC when the board layout's KV store holds none.
  static constexpr uint8_t SDK_DEFAULT_MAC[MAC_ADDRESS_SIZE] = {0x00, 0x50, 0xC2, 0x5E, 0x10, 0x88};
  uint8_t mac[MAC_ADDRESS_SIZE];
  get_mac_address_raw(mac);
  if (memcmp(mac, SDK_DEFAULT_MAC, MAC_ADDRESS_SIZE) == 0) {
    ESP_LOGE(TAG, "MAC is the LN882H SDK default 00:50:C2:5E:10:88; only one device with it can be on a network. "
                  "On a Tuya module, set board: to its Tuya board (e.g. wl2s, wl2h-u) and flash over serial");
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (this->version_ != nullptr) {
    this->version_->publish_state(&LT_BANNER_STR[10]);
  }
#endif  // USE_TEXT_SENSOR
}

float LTComponent::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace esphome::libretiny

#endif  // USE_LIBRETINY
