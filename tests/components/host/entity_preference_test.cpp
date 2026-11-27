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

static constexpr const char *TEST_SWITCH = "Test Switch";
static const uint32_t TEST_SWITCH_LEGACY_KEY = fnv1_hash_object_id(TEST_SWITCH, strlen(TEST_SWITCH), true);
static const uint32_t TEST_SWITCH_KEY = fnv1_hash_bytes(TEST_SWITCH, strlen(TEST_SWITCH));

/// Same entity_key_ means same preference key.
class TestEntity : public EntityBase {
 public:
  void set_test_id(uint32_t hash) { this->entity_key_ = hash; }
  void configure(const char *name) { this->configure_entity_(name, 0, 0); }
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
    // Clear the in-memory store so values do not leak between tests.
    host_preferences->reset();
  }

  void TearDown() override {
    unsetenv("ESPHOME_PREFDIR");
    fs::remove_all(this->temp_dir_);
  }

  fs::path temp_dir_;
};

// in_flash must not change the key; host ignores the storage type, so only the key is checked.
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

// Data under the legacy object_id-hash key moves to the raw-name key on first use
TEST_F(EntityPreferenceTest, LegacyKeyDataIsMigrated) {
  TestEntity entity;
  entity.configure(TEST_SWITCH);
  const uint32_t legacy_key = TEST_SWITCH_LEGACY_KEY ^ 3;
  const uint32_t new_key = TEST_SWITCH_KEY ^ 3;
  const uint32_t saved = 0xBEEF;
  ASSERT_TRUE(global_preferences->make_preference<uint32_t>(legacy_key).save(&saved));

  uint32_t loaded = 0;
  ASSERT_TRUE(entity.make_entity_preference<uint32_t>(3).load(&loaded));
  EXPECT_EQ(loaded, saved);
  loaded = 0;
  ASSERT_TRUE(global_preferences->make_preference<uint32_t>(new_key).load(&loaded));
  EXPECT_EQ(loaded, saved) << "the data must now be stored under the new key";
}

TEST_F(EntityPreferenceTest, NewKeyDataIsNeverOverwritten) {
  TestEntity entity;
  entity.configure(TEST_SWITCH);
  const uint32_t stale = 1;
  const uint32_t current = 2;
  ASSERT_TRUE(global_preferences->make_preference<uint32_t>(TEST_SWITCH_LEGACY_KEY).save(&stale));
  ASSERT_TRUE(global_preferences->make_preference<uint32_t>(TEST_SWITCH_KEY).save(&current));

  uint32_t loaded = 0;
  ASSERT_TRUE(entity.make_entity_preference<uint32_t>(0, true).load(&loaded));
  EXPECT_EQ(loaded, current);
}

}  // namespace esphome::host::testing
#endif  // USE_HOST
