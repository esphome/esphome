#ifdef USE_HOST
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include "esphome/components/host/preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"

namespace esphome::host::testing {
namespace fs = std::filesystem;

/// The preference key is derived from object_id_hash_; two instances with the same value share stored state.
class TestEntity : public EntityBase {
 public:
  void set_test_id(uint32_t hash) { this->object_id_hash_ = hash; }
};

class EntityPreferenceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = fs::temp_directory_path() / "esphome_entity_preference_test";
    fs::remove_all(this->temp_dir_);
    fs::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    App.pre_setup("test_entity_pref", strlen("test_entity_pref"), "", 0);
    setup_preferences();
    // Host preferences also keep an in-memory store, which would leak between tests and GTEST_REPEAT iterations.
    host_preferences->reset();
  }

  void TearDown() override {
    unsetenv("ESPHOME_PREFDIR");
    fs::remove_all(this->temp_dir_);
  }

  fs::path temp_dir_;
};

// The in_flash overload must use the same key as make_entity_preference(version), so choosing a storage never
// orphans a value saved without one. Host ignores flash vs RTC, so only the key is observable here.
TEST_F(EntityPreferenceTest, InFlashOverloadKeepsTheEntityKey) {
  TestEntity entity;
  entity.set_test_id(0x6001);
  for (const bool in_flash : {false, true}) {
    const uint32_t saved = in_flash ? 0xF1A5 : 0x0C7C;
    auto with_storage = entity.make_entity_preference<uint32_t>(7, in_flash);
    ASSERT_TRUE(with_storage.save(&saved));
    global_preferences->sync();

    uint32_t loaded = 0;
    auto without_storage = entity.make_entity_preference<uint32_t>(7);
    ASSERT_TRUE(without_storage.load(&loaded)) << "in_flash=" << in_flash;
    EXPECT_EQ(loaded, saved) << "in_flash=" << in_flash;
  }
}

TEST_F(EntityPreferenceTest, InFlashOverloadStillAppliesTheVersion) {
  TestEntity entity;
  entity.set_test_id(0x6002);
  const uint32_t saved = 42;
  auto v1 = entity.make_entity_preference<uint32_t>(1, true);
  ASSERT_TRUE(v1.save(&saved));
  global_preferences->sync();

  uint32_t loaded = 0;
  auto v2 = entity.make_entity_preference<uint32_t>(2, true);
  EXPECT_FALSE(v2.load(&loaded)) << "a different version must give a different key";
}

}  // namespace esphome::host::testing
#endif  // USE_HOST
