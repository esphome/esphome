#include "preferences.h"
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp8266 {

static const char *const TAG = "esp8266.preferences";

class ESP8266Preferences : public ESPPreferences {
 public:
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return make_preference(length, type);
  }
  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    return ESPPreferenceObject(nullptr);
  }
};

void setup_preferences() {
  // pass
  global_preferences = new ESP8266Preferences();
}
void preferences_prevent_write(bool prevent) {
  // TODO
}

}  // namespace esp8266

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome
