// Pins the preferences contract concepts so the surface they enforce cannot
// drift unnoticed: a minimal conforming type must satisfy each concept, and a
// type missing a method or returning the wrong type must not.

#include <gtest/gtest.h>

#include "esphome/core/preference_backend.h"

namespace esphome::core::testing {

struct MinimalBackend {
  bool save(const uint8_t *, size_t) { return true; }
  bool load(uint8_t *, size_t) { return true; }
};
static_assert(PreferenceBackendContract<MinimalBackend>);

struct BackendMissingLoad {
  bool save(const uint8_t *, size_t) { return true; }
};
static_assert(!PreferenceBackendContract<BackendMissingLoad>);

struct BackendWrongReturn {
  void save(const uint8_t *, size_t) {}
  bool load(uint8_t *, size_t) { return true; }
};
static_assert(!PreferenceBackendContract<BackendWrongReturn>);

struct MinimalPreferences {
  ESPPreferenceObject make_preference(size_t, uint32_t, bool) { return {}; }
  ESPPreferenceObject make_preference(size_t, uint32_t) { return {}; }
  bool sync() { return true; }
  bool reset() { return true; }
};
static_assert(PreferencesContract<MinimalPreferences>);

struct PreferencesMissingTwoArgForm {
  ESPPreferenceObject make_preference(size_t, uint32_t, bool) { return {}; }
  bool sync() { return true; }
  bool reset() { return true; }
};
static_assert(!PreferencesContract<PreferencesMissingTwoArgForm>);

struct PreferencesMissingReset {
  ESPPreferenceObject make_preference(size_t, uint32_t, bool) { return {}; }
  ESPPreferenceObject make_preference(size_t, uint32_t) { return {}; }
  bool sync() { return true; }
};
static_assert(!PreferencesContract<PreferencesMissingReset>);

TEST(PreferenceContract, NullBackendRefusesBothOperations) {
  // ESPPreferenceObject forwards to whichever backend the platform binds; a
  // default-constructed object has no backend and must refuse both operations
  // instead of crashing.
  ESPPreferenceObject without_backend;
  uint32_t value = 42;
  EXPECT_FALSE(without_backend.save(&value));
  EXPECT_FALSE(without_backend.load(&value));
}

}  // namespace esphome::core::testing
