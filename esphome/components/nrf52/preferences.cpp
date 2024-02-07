#ifdef USE_NRF52

#include "esphome/core/preferences.h"

namespace esphome {
namespace nrf52 {

class NRF52Preferences : public ESPPreferences {
 public:
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return make_preference(length, type);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    // TODO
    return {};
  }

  bool sync() override {
    // TODO
    return true;
  }

  bool reset() override {
    // TODO
    return true;
  }

  //  protected:
  //   uint8_t *eeprom_sector_;
};

void setup_preferences() {
  auto *prefs = new NRF52Preferences();  // NOLINT(cppcoreguidelines-owning-memory)
  global_preferences = prefs;
}
// TODO
//  void preferences_prevent_write(bool prevent) { s_prevent_write = prevent; }

}  // namespace nrf52

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif  // USE_RP2040
