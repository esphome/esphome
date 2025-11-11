
#ifdef USE_ESP32
#include "psram.h"
#include <esp_idf_version.h>
#include <esp_psram.h>

#include "esphome/core/log.h"

#include <esp_heap_caps.h>

namespace esphome {
namespace psram {
static const char *const TAG = "psram";

void PsramComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "PSRAM:");
  bool available = esp_psram_is_initialized();

  ESP_LOGCONFIG(TAG, "  Available: %s", YESNO(available));
  if (available) {
    ESP_LOGCONFIG(TAG, "  Size: %zu KB", esp_psram_get_size() / 1024);
#if CONFIG_SPIRAM_ECC_ENABLE
    ESP_LOGCONFIG(TAG, "  ECC enabled: YES");
#endif
  }
}

}  // namespace psram
}  // namespace esphome

#endif
