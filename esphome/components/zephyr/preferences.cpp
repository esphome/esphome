#ifdef USE_ZEPHYR

#include <zephyr/kernel.h>
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include <zephyr/settings/settings.h>

namespace esphome {
namespace zephyr {

static const char *const TAG = "zephyr.preferences";

#define ESPHOME_SETTINGS_KEY "esphome"

void setup_preferences() {}

}  // namespace zephyr

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif
