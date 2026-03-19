#pragma once
#ifdef USE_ZEPHYR
#ifdef CONFIG_SETTINGS

#include "esphome/core/preference_backend.h"
#include <zephyr/settings/settings.h>
#include <vector>

namespace esphome::zephyr {

class ZephyrPreferences final : public PreferencesMixin<ZephyrPreferences> {
 public:
  using PreferencesMixin<ZephyrPreferences>::make_preference;
  void open();
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) {
    return this->make_preference(length, type);
  }
  ESPPreferenceObject make_preference(size_t length, uint32_t type);
  bool sync();
  bool reset();

 protected:
  std::vector<ZephyrPreferenceBackend *> backends_;

  static int load_setting(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg);
  static int export_settings(int (*cb)(const char *name, const void *value, size_t val_len));
};

void setup_preferences();

}  // namespace esphome::zephyr

DECLARE_PREFERENCE_ALIASES(esphome::zephyr::ZephyrPreferences)

#endif  // CONFIG_SETTINGS
#endif  // USE_ZEPHYR
