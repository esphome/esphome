#ifdef USE_HOST
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include "common.h"
#include "esphome/components/host/preferences.h"
#include "esphome/components/number/number.h"
#include "esphome/core/application.h"

namespace esphome::ld2450::testing {
namespace fs = std::filesystem;

namespace {
TestableLD2450 *hub = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// What the generated apply="set_presence_timeout()", publish="before" number does
void apply_presence_timeout(number::Number *entity, float value) {
  entity->publish_state(value);
  hub->set_presence_timeout();
}
using PresenceTimeoutNumber = number::ApplyNumber<apply_presence_timeout>;

// The presence_timeout part of LD2450Component::setup(), without its radar restart and scheduled info read
void boot(TestableLD2450 &radar, number::Number *entity) {
  radar.set_presence_timeout_number(entity);
  if (entity != nullptr) {
    radar.pref_ = entity->make_entity_preference<float>();
    radar.set_presence_timeout();
  }
}
}  // namespace

class LD2450PresenceTimeoutTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = fs::temp_directory_path() / "esphome_ld2450_presence_timeout_test";
    fs::remove_all(this->temp_dir_);
    fs::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    App.pre_setup("test_ld2450_timeout", strlen("test_ld2450_timeout"), "", 0);
    // A fresh store per test: the shared one keeps the file path of whichever test set it up first, whose directory
    // may already be gone, and its in-memory values
    host::host_preferences = &this->prefs_;
    global_preferences = &this->prefs_;
    this->ld2450_.set_uart_parent(&this->uart_);
    hub = &this->ld2450_;
  }

  void TearDown() override {
    hub = nullptr;
    host::setup_preferences();
    unsetenv("ESPHOME_PREFDIR");
    fs::remove_all(this->temp_dir_);
  }

  /// Save a presence_timeout the way a previous boot did, and flush it
  void save_timeout(float seconds) {
    PresenceTimeoutNumber saved;
    ESPPreferenceObject pref = saved.make_entity_preference<float>();
    ASSERT_TRUE(pref.save(&seconds));
    ASSERT_TRUE(global_preferences->sync());
  }

  /// Whether a frame without a target, now ms after the last one, clears presence
  bool cleared_after(uint32_t last_target_ms, uint32_t now_ms) {
    LoopBlockingGuard clock{nullptr, nullptr, now_ms};
    return this->ld2450_.get_timeout_status_(last_target_ms);
  }

  host::HostPreferences prefs_;
  MockUARTComponent uart_;
  TestableLD2450 ld2450_;
  PresenceTimeoutNumber timeout_number_;
  fs::path temp_dir_;
};

TEST_F(LD2450PresenceTimeoutTest, NoNumberKeepsDefaultHold) {
  boot(this->ld2450_, nullptr);

  EXPECT_EQ(this->ld2450_.timeout_, 5000u);
  EXPECT_FALSE(this->cleared_after(1000, 5999));
  EXPECT_TRUE(this->cleared_after(1000, 6000));
}

TEST_F(LD2450PresenceTimeoutTest, SetupWithNothingSavedPublishesDefault) {
  boot(this->ld2450_, &this->timeout_number_);

  ASSERT_TRUE(this->timeout_number_.has_state());
  EXPECT_EQ(this->timeout_number_.state, 5.0f);
  EXPECT_EQ(this->ld2450_.timeout_, 5000u);
}

TEST_F(LD2450PresenceTimeoutTest, SetupWithSavedZeroPublishesZero) {
  this->save_timeout(0);
  boot(this->ld2450_, &this->timeout_number_);

  ASSERT_TRUE(this->timeout_number_.has_state());
  EXPECT_EQ(this->timeout_number_.state, 0.0f);
  EXPECT_EQ(this->ld2450_.timeout_, 0u);
}

TEST_F(LD2450PresenceTimeoutTest, SetupWithSavedValuePublishesIt) {
  this->save_timeout(12);
  boot(this->ld2450_, &this->timeout_number_);

  EXPECT_EQ(this->timeout_number_.state, 12.0f);
  EXPECT_EQ(this->ld2450_.timeout_, 12000u);
}

TEST_F(LD2450PresenceTimeoutTest, ControlZeroKeepsZeroAndHoldsNothing) {
  boot(this->ld2450_, &this->timeout_number_);
  ASSERT_EQ(this->timeout_number_.state, 5.0f);

  auto call = this->timeout_number_.make_call();
  call.set_value(0);
  call.perform();

  // The old code restored the saved 5 s as soon as the state read 0
  EXPECT_EQ(this->timeout_number_.state, 0.0f);
  EXPECT_EQ(this->ld2450_.timeout_, 0u);
  EXPECT_TRUE(this->cleared_after(1000, 1000));
  EXPECT_TRUE(this->cleared_after(1000, 1001));
}

TEST_F(LD2450PresenceTimeoutTest, ControlZeroIsSavedForTheNextBoot) {
  boot(this->ld2450_, &this->timeout_number_);

  auto call = this->timeout_number_.make_call();
  call.set_value(0);
  call.perform();
  ASSERT_TRUE(global_preferences->sync());

  TestableLD2450 rebooted;
  PresenceTimeoutNumber rebooted_number;
  rebooted.set_uart_parent(&this->uart_);
  hub = &rebooted;
  boot(rebooted, &rebooted_number);

  ASSERT_TRUE(rebooted_number.has_state());
  EXPECT_EQ(rebooted_number.state, 0.0f);
  EXPECT_EQ(rebooted.timeout_, 0u);
}

TEST_F(LD2450PresenceTimeoutTest, ControlNonZeroSetsHold) {
  boot(this->ld2450_, &this->timeout_number_);

  auto call = this->timeout_number_.make_call();
  call.set_value(3);
  call.perform();

  EXPECT_EQ(this->ld2450_.timeout_, 3000u);
  EXPECT_FALSE(this->cleared_after(1000, 3999));
  EXPECT_TRUE(this->cleared_after(1000, 4000));
}

}  // namespace esphome::ld2450::testing
#endif  // USE_HOST
