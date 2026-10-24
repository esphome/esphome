#ifdef USE_HOST
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include "esphome/components/host/preferences.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/application.h"

namespace esphome::switch_::testing {
namespace fs = std::filesystem;

/// Switch is abstract; this is the minimum needed to instantiate one.
class TestSwitch : public Switch {
 public:
  void write_state(bool state) override {}
  /// The preference key is derived from object_id_hash_; two instances with the same value share stored state.
  void set_test_id(uint32_t hash) { this->object_id_hash_ = hash; }
};

class SwitchRestoreStorageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = fs::temp_directory_path() / "esphome_switch_storage_test";
    fs::remove_all(this->temp_dir_);
    fs::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    App.pre_setup("test_switch_storage", strlen("test_switch_storage"), "", 0);
    host::setup_preferences();
    // Host preferences also keep an in-memory store, which would leak between tests and GTEST_REPEAT iterations.
    host::host_preferences->reset();
  }

  void TearDown() override {
    unsetenv("ESPHOME_PREFDIR");
    fs::remove_all(this->temp_dir_);
  }

  fs::path temp_dir_;
};

// Host ignores flash vs RTC, so this checks that a switch with a storage bit in its restore mode still binds,
// saves and restores its preference; which memory it lands in is checked by the ESP32/ESP8266 compile tests.
TEST_F(SwitchRestoreStorageTest, SwitchWithAStorageChoiceStillRestoresItsState) {
  static constexpr uint32_t ID = 0x5001;
  const auto mode = static_cast<SwitchRestoreMode>(SWITCH_RESTORE_DEFAULT_OFF | RESTORE_MODE_STORAGE_RTC_MASK);
  {
    TestSwitch sw;
    sw.set_test_id(ID);
    sw.set_restore_mode(mode);
    auto first = sw.get_initial_state_with_restore_mode();
    ASSERT_TRUE(first.has_value());
    EXPECT_FALSE(first.value()) << "nothing stored yet, so RESTORE_DEFAULT_OFF gives OFF";
    sw.publish_state(true);
    global_preferences->sync();
  }
  TestSwitch sw2;
  sw2.set_test_id(ID);
  sw2.set_restore_mode(mode);
  auto restored = sw2.get_initial_state_with_restore_mode();
  ASSERT_TRUE(restored.has_value());
  EXPECT_TRUE(restored.value()) << "the saved state was not restored";
}

}  // namespace esphome::switch_::testing
#endif  // USE_HOST
