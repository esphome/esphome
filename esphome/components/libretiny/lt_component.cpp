#include "lt_component.h"

#ifdef USE_LIBRETINY

#include <cinttypes>
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

#if defined(USE_BK72XX) && defined(USE_WIFI)
  // The SDK silently runs the radio uncalibrated when the layout's calibration partition lacks the TLV table.
  static constexpr uint32_t RF_CAL_TLV_MAGIC = 0x00564C54;  // "TLV\0" read as little endian
  static constexpr uint32_t RF_CAL_OFFSETS[] = {FLASH_CALIBRATION_OFFSET, 0x1D0000, 0x1E0000, 0x1E3000};
  uint32_t found = 0;
  for (uint32_t offset : RF_CAL_OFFSETS) {
    uint32_t magic;
    if (lt_flash_read(offset, (uint8_t *) &magic, sizeof(magic)) == sizeof(magic) && magic == RF_CAL_TLV_MAGIC) {
      found = offset;
      break;
    }
  }
  if (found == 0) {
    ESP_LOGE(TAG, "No RF calibration found; Wi-Fi may be unreliable. Check that board: matches this module");
  } else if (found != FLASH_CALIBRATION_OFFSET) {
    ESP_LOGE(TAG,
             "board: layout wrong for this module, RF calibration is at 0x%06" PRIX32 " not 0x%06" PRIX32
             "; reflash over serial",
             found, (uint32_t) FLASH_CALIBRATION_OFFSET);
  }
#endif

#ifdef USE_TEXT_SENSOR
  PUBLISH_STATE(this->version_text_sensor_, &LT_BANNER_STR[10]);
#endif  // USE_TEXT_SENSOR
}

float LTComponent::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace esphome::libretiny

#endif  // USE_LIBRETINY
