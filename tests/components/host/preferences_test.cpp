#ifdef USE_HOST
#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include "esphome/components/host/preferences.h"
#include "esphome/core/application.h"

namespace esphome::host::testing {
namespace fs = std::filesystem;

/// RAII helper to save and restore an environment variable.
class ScopedEnvVar {
 public:
  explicit ScopedEnvVar(const char *name) : name_(name) {
    const char *val = getenv(name);
    if (val != nullptr) {
      saved_value_ = val;
      was_set_ = true;
    }
  }
  ~ScopedEnvVar() {
    if (this->was_set_) {
      setenv(this->name_.c_str(), this->saved_value_.c_str(), 1);
    } else {
      unsetenv(this->name_.c_str());
    }
  }
  ScopedEnvVar(const ScopedEnvVar &) = delete;
  ScopedEnvVar &operator=(const ScopedEnvVar &) = delete;

 private:
  std::string name_;
  std::string saved_value_;
  bool was_set_{false};
};

class HostPreferencesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a unique temp directory for this test
    this->temp_dir_ = fs::temp_directory_path() / "esphome_prefs_test";
    fs::create_directories(this->temp_dir_);

    // Set up App name — string literal has static storage so StringRef is safe
    App.pre_setup("test_prefs", 10, "", 0);
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(this->temp_dir_, ec);
  }

  fs::path temp_dir_;
};

TEST_F(HostPreferencesTest, BothVarsUnset_SyncReturnsFalse) {
  ScopedEnvVar home_guard("HOME");
  ScopedEnvVar prefdir_guard("ESPHOME_PREFDIR");
  unsetenv("HOME");
  unsetenv("ESPHOME_PREFDIR");

  HostPreferences prefs;
  EXPECT_FALSE(prefs.sync());
}

TEST_F(HostPreferencesTest, BothVarsUnset_SaveSucceedsInMemory) {
  ScopedEnvVar home_guard("HOME");
  ScopedEnvVar prefdir_guard("ESPHOME_PREFDIR");
  unsetenv("HOME");
  unsetenv("ESPHOME_PREFDIR");

  HostPreferences prefs;
  uint32_t value = 42;
  // save() stores in memory even without a valid file path
  EXPECT_TRUE(prefs.save(0x1234, reinterpret_cast<const uint8_t *>(&value), sizeof(value)));

  // But sync to disk should fail
  EXPECT_FALSE(prefs.sync());
}

TEST_F(HostPreferencesTest, PrefDirSet_SaveAndSync) {
  ScopedEnvVar home_guard("HOME");
  ScopedEnvVar prefdir_guard("ESPHOME_PREFDIR");

  auto prefdir = this->temp_dir_ / "prefdir";
  setenv("ESPHOME_PREFDIR", prefdir.c_str(), 1);
  unsetenv("HOME");

  HostPreferences prefs;
  uint32_t value = 42;
  EXPECT_TRUE(prefs.save(0x1234, reinterpret_cast<const uint8_t *>(&value), sizeof(value)));
  EXPECT_TRUE(prefs.sync());

  // Verify file was created in ESPHOME_PREFDIR
  auto expected_file = prefdir / "test_prefs.prefs";
  EXPECT_TRUE(fs::exists(expected_file));
}

TEST_F(HostPreferencesTest, HomeSet_SaveAndSync) {
  ScopedEnvVar home_guard("HOME");
  ScopedEnvVar prefdir_guard("ESPHOME_PREFDIR");

  auto home = this->temp_dir_ / "home";
  setenv("HOME", home.c_str(), 1);
  unsetenv("ESPHOME_PREFDIR");

  HostPreferences prefs;
  uint32_t value = 42;
  EXPECT_TRUE(prefs.save(0x1234, reinterpret_cast<const uint8_t *>(&value), sizeof(value)));
  EXPECT_TRUE(prefs.sync());

  // Verify file was created in HOME/.esphome/prefs
  auto expected_file = home / ".esphome" / "prefs" / "test_prefs.prefs";
  EXPECT_TRUE(fs::exists(expected_file));
}

TEST_F(HostPreferencesTest, PrefDirTakesPrecedenceOverHome) {
  ScopedEnvVar home_guard("HOME");
  ScopedEnvVar prefdir_guard("ESPHOME_PREFDIR");

  auto prefdir = this->temp_dir_ / "prefdir";
  auto home = this->temp_dir_ / "home";
  setenv("ESPHOME_PREFDIR", prefdir.c_str(), 1);
  setenv("HOME", home.c_str(), 1);

  HostPreferences prefs;
  uint32_t value = 42;
  EXPECT_TRUE(prefs.save(0x1234, reinterpret_cast<const uint8_t *>(&value), sizeof(value)));
  EXPECT_TRUE(prefs.sync());

  // File should be in ESPHOME_PREFDIR, not HOME
  auto prefdir_file = prefdir / "test_prefs.prefs";
  auto home_file = home / ".esphome" / "prefs" / "test_prefs.prefs";
  EXPECT_TRUE(fs::exists(prefdir_file));
  EXPECT_FALSE(fs::exists(home_file));
}

TEST_F(HostPreferencesTest, SaveAndLoadRoundTrip) {
  ScopedEnvVar prefdir_guard("ESPHOME_PREFDIR");

  auto prefdir = this->temp_dir_ / "roundtrip";
  setenv("ESPHOME_PREFDIR", prefdir.c_str(), 1);

  // Save data with one instance
  {
    HostPreferences prefs;
    uint32_t value = 0xDEADBEEF;
    EXPECT_TRUE(prefs.save(0xABCD, reinterpret_cast<const uint8_t *>(&value), sizeof(value)));
    EXPECT_TRUE(prefs.sync());
  }

  // Load with a fresh instance (reads from file)
  {
    HostPreferences prefs;
    uint32_t loaded = 0;
    EXPECT_TRUE(prefs.load(0xABCD, reinterpret_cast<uint8_t *>(&loaded), sizeof(loaded)));
    EXPECT_EQ(loaded, 0xDEADBEEFu);
  }
}

TEST_F(HostPreferencesTest, LoadNonExistentKeyReturnsFalse) {
  ScopedEnvVar prefdir_guard("ESPHOME_PREFDIR");

  auto prefdir = this->temp_dir_ / "nokey";
  setenv("ESPHOME_PREFDIR", prefdir.c_str(), 1);

  HostPreferences prefs;
  uint32_t loaded = 0;
  EXPECT_FALSE(prefs.load(0x9999, reinterpret_cast<uint8_t *>(&loaded), sizeof(loaded)));
}

}  // namespace esphome::host::testing

#endif
