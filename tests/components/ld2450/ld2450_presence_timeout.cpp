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
class PresenceTimeoutLD2450 : public TestableLD2450 {
 public:
  using LD2450Component::get_timeout_status_;
  using LD2450Component::timeout_;
};

PresenceTimeoutLD2450 *hub = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// What the generated apply="set_presence_timeout()", publish="before" number does
void apply_presence_timeout(number::Number *entity, float value) {
  entity->publish_state(value);
  hub->set_presence_timeout();
}
using PresenceTimeoutNumber = number::ApplyNumber<apply_presence_timeout>;
}  // namespace

class LD2450PresenceTimeoutTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = fs::temp_directory_path() / "esphome_ld2450_presence_timeout_test";
    fs::remove_all(this->temp_dir_);
    fs::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    App.pre_setup("test_ld2450_timeout", strlen("test_ld2450_timeout"), "", 0);
    host::setup_preferences();
    // Host preferences also keep an in-memory store, which would leak between tests
    host::host_preferences->reset();
    this->ld2450_.set_uart_parent(&this->uart_);
    hub = &this->ld2450_;
  }

  // setup() leaves the anonymous 1500 ms info-read timeout of its radar restart on the component in the shared App
  // scheduler; it cannot be cancelled, so run it while the fixture is still alive
  void TearDown() override {
    const uint32_t start = millis();
    App.scheduler.process_to_add();
    while (App.scheduler.next_schedule_in(millis()).has_value() && millis() - start < 3000) {
      App.scheduler.call(millis());
      delay(1);
      App.scheduler.process_to_add();
    }
    App.scheduler.trim_freelist();
    hub = nullptr;
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

  MockUARTComponent uart_;
  PresenceTimeoutLD2450 ld2450_;
  PresenceTimeoutNumber timeout_number_;
  // A second boot in the same test; a member so its scheduled timeout runs in TearDown() while it is alive
  PresenceTimeoutLD2450 rebooted_;
  PresenceTimeoutNumber rebooted_number_;
  fs::path temp_dir_;
};

TEST_F(LD2450PresenceTimeoutTest, NoNumberKeepsDefaultHold) {
  this->ld2450_.setup();

  EXPECT_EQ(this->ld2450_.timeout_, 5000u);
  EXPECT_FALSE(this->cleared_after(1000, 5999));
  EXPECT_TRUE(this->cleared_after(1000, 6000));
}

TEST_F(LD2450PresenceTimeoutTest, SetupWithNothingSavedPublishesDefault) {
  this->ld2450_.set_presence_timeout_number(&this->timeout_number_);
  this->ld2450_.setup();

  ASSERT_TRUE(this->timeout_number_.has_state());
  EXPECT_EQ(this->timeout_number_.state, 5.0f);
  EXPECT_EQ(this->ld2450_.timeout_, 5000u);
}

TEST_F(LD2450PresenceTimeoutTest, SetupWithSavedZeroPublishesZero) {
  this->save_timeout(0);
  this->ld2450_.set_presence_timeout_number(&this->timeout_number_);
  this->ld2450_.setup();

  ASSERT_TRUE(this->timeout_number_.has_state());
  EXPECT_EQ(this->timeout_number_.state, 0.0f);
  EXPECT_EQ(this->ld2450_.timeout_, 0u);
}

TEST_F(LD2450PresenceTimeoutTest, SetupWithSavedValuePublishesIt) {
  this->save_timeout(12);
  this->ld2450_.set_presence_timeout_number(&this->timeout_number_);
  this->ld2450_.setup();

  EXPECT_EQ(this->timeout_number_.state, 12.0f);
  EXPECT_EQ(this->ld2450_.timeout_, 12000u);
}

TEST_F(LD2450PresenceTimeoutTest, ControlZeroKeepsZeroAndHoldsNothing) {
  this->ld2450_.set_presence_timeout_number(&this->timeout_number_);
  this->ld2450_.setup();
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
  this->ld2450_.set_presence_timeout_number(&this->timeout_number_);
  this->ld2450_.setup();

  auto call = this->timeout_number_.make_call();
  call.set_value(0);
  call.perform();
  ASSERT_TRUE(global_preferences->sync());

  this->rebooted_.set_uart_parent(&this->uart_);
  hub = &this->rebooted_;
  this->rebooted_.set_presence_timeout_number(&this->rebooted_number_);
  this->rebooted_.setup();

  ASSERT_TRUE(this->rebooted_number_.has_state());
  EXPECT_EQ(this->rebooted_number_.state, 0.0f);
  EXPECT_EQ(this->rebooted_.timeout_, 0u);
}

TEST_F(LD2450PresenceTimeoutTest, ControlNonZeroSetsHold) {
  this->ld2450_.set_presence_timeout_number(&this->timeout_number_);
  this->ld2450_.setup();

  auto call = this->timeout_number_.make_call();
  call.set_value(3);
  call.perform();

  EXPECT_EQ(this->ld2450_.timeout_, 3000u);
  EXPECT_FALSE(this->cleared_after(1000, 3999));
  EXPECT_TRUE(this->cleared_after(1000, 4000));
}

}  // namespace esphome::ld2450::testing
#endif  // USE_HOST
