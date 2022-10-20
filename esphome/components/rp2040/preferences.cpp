#ifdef USE_RP2040

#include "preferences.h"

#include <cstring>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace rp2040 {

static const char *const TAG = "rp2040.preferences";

class RP2040PreferenceBackend : public ESPPreferenceBackend {
 public:
  bool save(const uint8_t *data, size_t len) override { return true; }
  bool load(uint8_t *data, size_t len) override { return false; }
};

class RP2040Preferences : public ESPPreferences {
 public:
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    auto *pref = new RP2040PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    return ESPPreferenceObject(pref);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    auto *pref = new RP2040PreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    return ESPPreferenceObject(pref);
  }

  bool sync() override { return true; }

  bool reset() override { return true; }
};

void setup_preferences() {
  auto *prefs = new RP2040Preferences();  // NOLINT(cppcoreguidelines-owning-memory)
  global_preferences = prefs;
}

}  // namespace rp2040

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_RP2040
