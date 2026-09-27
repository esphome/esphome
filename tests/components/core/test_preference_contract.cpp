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

struct MinimalPreferences : public PreferencesMixin<MinimalPreferences> {
  using PreferencesMixin<MinimalPreferences>::make_preference;
  ESPPreferenceObject make_preference(size_t, uint32_t, bool) { return {}; }
  ESPPreferenceObject make_preference(size_t, uint32_t) { return {}; }
  bool sync() { return true; }
  bool reset() { return true; }
};
static_assert(PreferencesContract<MinimalPreferences>);

struct PreferencesMissingTwoArgForm : public PreferencesMixin<PreferencesMissingTwoArgForm> {
  using PreferencesMixin<PreferencesMissingTwoArgForm>::make_preference;
  ESPPreferenceObject make_preference(size_t, uint32_t, bool) { return {}; }
  bool sync() { return true; }
  bool reset() { return true; }
};
static_assert(!PreferencesContract<PreferencesMissingTwoArgForm>);

struct PreferencesMissingReset : public PreferencesMixin<PreferencesMissingReset> {
  using PreferencesMixin<PreferencesMissingReset>::make_preference;
  ESPPreferenceObject make_preference(size_t, uint32_t, bool) { return {}; }
  ESPPreferenceObject make_preference(size_t, uint32_t) { return {}; }
  bool sync() { return true; }
};
static_assert(!PreferencesContract<PreferencesMissingReset>);

struct PreferencesWrongSyncReturn : public PreferencesMixin<PreferencesWrongSyncReturn> {
  using PreferencesMixin<PreferencesWrongSyncReturn>::make_preference;
  ESPPreferenceObject make_preference(size_t, uint32_t, bool) { return {}; }
  ESPPreferenceObject make_preference(size_t, uint32_t) { return {}; }
  void sync() {}
  bool reset() { return true; }
};
static_assert(!PreferencesContract<PreferencesWrongSyncReturn>);

// Forgot `using PreferencesMixin<X>::make_preference;`, so the derived
// overloads hide the template forms (see the PreferencesContract note in
// preference_backend.h); the concept must reject the class.
struct PreferencesForgotUsingDeclaration : public PreferencesMixin<PreferencesForgotUsingDeclaration> {
  ESPPreferenceObject make_preference(size_t, uint32_t, bool) { return {}; }
  ESPPreferenceObject make_preference(size_t, uint32_t) { return {}; }
  bool sync() { return true; }
  bool reset() { return true; }
};
static_assert(!PreferencesContract<PreferencesForgotUsingDeclaration>);

struct MinimalKeyLookup {
  bool load_from_key(uint32_t, uint8_t *, size_t) { return true; }
};
static_assert(PreferencesKeyLookupContract<MinimalKeyLookup>);

struct KeyLookupMissingMethod {};
static_assert(!PreferencesKeyLookupContract<KeyLookupMissingMethod>);

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
