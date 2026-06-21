#pragma once
#ifdef USE_ESP32

#include "esphome/core/preference_backend.h"
#include <soc/soc_caps.h>

namespace esphome::esp32 {

struct NVSData;

class ESP32Preferences final : public PreferencesMixin<ESP32Preferences> {
 public:
  using PreferencesMixin<ESP32Preferences>::make_preference;
  void open();
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash);
  // Two-argument form defaults to NVS (flash) storage, preserving historic ESP32 behavior.
  ESPPreferenceObject make_preference(size_t length, uint32_t type);
  bool sync();
  bool reset();

  uint32_t nvs_handle;

 protected:
  bool is_changed_(uint32_t nvs_handle, const NVSData &to_save, const char *key_str);

#if SOC_RTC_MEM_SUPPORTED
  // RTC-backed storage (in_flash=false). Unavailable on variants without RTC memory (C2, C61).
  ESPPreferenceObject make_rtc_preference_(size_t length, uint32_t type);
  // Next free word offset in the RTC storage region (bump allocated in make_preference order).
  uint16_t current_rtc_offset_{0};
#endif
};

void setup_preferences();

}  // namespace esphome::esp32

DECLARE_PREFERENCE_ALIASES(esphome::esp32::ESP32Preferences)

#endif  // USE_ESP32
