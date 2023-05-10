#ifdef USE_HOST

#include "preferences.h"
#include <cstring>
#include "esphome/core/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace host {

static const char *const TAG = "host.preferences";

class HostPreferences : public ESPPreferences {
 public:
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override { return {}; }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override { return {}; }

  bool sync() override { return true; }
  bool reset() override { return true; }
};

void setup_preferences() {
  auto *pref = new HostPreferences();  // NOLINT(cppcoreguidelines-owning-memory)
  global_preferences = pref;
}

}  // namespace host

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_HOST
