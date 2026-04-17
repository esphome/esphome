#pragma once

#include "esphome/core/preference_backend.h"

// Include the concrete preferences manager for the active platform.
// Each header defines its manager class and provides the Preferences,
// ESPPreferences, and global_preferences declarations.
#ifdef USE_ESP32
#include "esphome/components/esp32/preferences.h"
#elif defined(USE_ESP8266)
#include "esphome/components/esp8266/preferences.h"
#elif defined(USE_RP2040)
#include "esphome/components/rp2040/preferences.h"
#elif defined(USE_LIBRETINY)
#include "esphome/components/libretiny/preferences.h"
#elif defined(USE_HOST)
#include "esphome/components/host/preferences.h"
#elif defined(USE_ZEPHYR) && defined(CONFIG_SETTINGS)
#include "esphome/components/zephyr/preferences.h"
#else
namespace esphome {
struct Preferences : public PreferencesMixin<Preferences> {
  using PreferencesMixin<Preferences>::make_preference;
  ESPPreferenceObject make_preference(size_t, uint32_t, bool) { return {}; }
  ESPPreferenceObject make_preference(size_t, uint32_t) { return {}; }

  /**
   * Commit pending writes to flash.
   *
   * @return true if write is successful.
   */
  bool sync() { return false; }

  /**
   * Forget all unsaved changes and re-initialize the permanent preferences storage.
   * Usually followed by a restart which moves the system to "factory" conditions
   *
   * @return true if operation is successful.
   */
  bool reset() { return false; }
};
using ESPPreferences = Preferences;
extern ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace esphome
#endif
