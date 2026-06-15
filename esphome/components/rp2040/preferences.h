#pragma once
#ifdef USE_RP2040

#include "esphome/core/preference_backend.h"

namespace esphome::rp2040 {

class RP2040Preferences final : public PreferencesMixin<RP2040Preferences> {
 public:
  using PreferencesMixin<RP2040Preferences>::make_preference;
  RP2040Preferences();
  void setup();
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) {
    return this->make_preference(length, type);
  }
  ESPPreferenceObject make_preference(size_t length, uint32_t type);
  bool sync();
  bool reset();

  uint32_t current_flash_offset = 0;

 protected:
  uint8_t *eeprom_sector_;
};

void setup_preferences();
void preferences_prevent_write(bool prevent);

}  // namespace esphome::rp2040

DECLARE_PREFERENCE_ALIASES(esphome::rp2040::RP2040Preferences)

#endif  // USE_RP2040
