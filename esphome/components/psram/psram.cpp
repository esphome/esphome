#include "psram.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <esp_heap_caps.h>
#include <esp_idf_version.h>

namespace esphome {
namespace psram {

static const char *const TAG = "psram";

void PsramComponent::dump_config() {
  // Technically this can be false if the PSRAM is full, but heap_caps_get_total_size() isn't always available, and it's
  // very unlikely for the PSRAM to be full.
  bool available = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0;

  ESP_LOGCONFIG(TAG, "PSRAM:");
  ESP_LOGCONFIG(TAG, "  Available: %s", YESNO(available));
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
  if (available) {
    ESP_LOGCONFIG(TAG, "  Size: %d MB", heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024 / 1024);
  }
#endif
}

}  // namespace psram
}  // namespace esphome

#endif
