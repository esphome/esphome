#ifdef USE_HOST

#include <gtest/gtest.h>

#include <cmath>
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/filter_lifetime/filter_lifetime.h"
#include "esphome/components/host/preferences.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::filter_lifetime::testing {

class EntityIdentity : public EntityBase {
 public:
  static void configure(EntityBase &entity, const char *name, uint32_t hash) {
    auto configure = &EntityIdentity::configure_entity_;
    (entity.*configure)(name, hash, 0);
  }
};

class TestableFilterLifetime : public FilterLifetime {
 public:
  using FilterLifetime::last_update_;
  using FilterLifetime::runtime_seconds_;
};

static bool g_is_on = true;                   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static float g_speed = 100.0f;                // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static constexpr uint32_t MAX_MINUTES = 600;  // 10 hours
static constexpr uint32_t FILTER_HASH = 0x104001;

class FilterLifetimeTest : public ::testing::Test {
 protected:
  TestableFilterLifetime filter_;
  sensor::Sensor runtime_hours_;
  sensor::Sensor remaining_days_;
  float last_state_{NAN};
  unsigned publications_{0};

  void SetUp() override {
    new (&App) Application;
    App.pre_setup("filter-lifetime-tests", 21, "Filter lifetime tests", 21);
    host::setup_preferences();
    host::host_preferences->reset();
    g_is_on = true;
    g_speed = 100.0f;
    this->attach(this->filter_);
    this->filter_.add_on_state_callback([this](float value) {
      this->last_state_ = value;
      ++this->publications_;
    });
  }

  void TearDown() override {
    host::host_preferences->reset();
    App.~Application();
  }

  void attach(FilterLifetime &filter) {
    EntityIdentity::configure(filter, "Filter", FILTER_HASH);
    filter.set_max_lifetime(MAX_MINUTES);
    filter.set_is_on_lambda([]() { return g_is_on; });
    filter.set_current_speed_lambda([]() { return g_speed; });
    filter.set_runtime_hours_sensor(&this->runtime_hours_);
    filter.set_remaining_days_sensor(&this->remaining_days_);
  }

  // Run one update() as if elapsed_ms passed since the previous one
  void elapse(uint32_t elapsed_ms) {
    this->filter_.last_update_ = App.get_loop_component_start_time() - elapsed_ms;
    this->filter_.update();
  }

  bool stored(uint32_t &value) { return this->filter_.make_entity_preference<uint32_t>().load(&value); }

  static float expected_percent(uint32_t runtime_seconds) {
    return 100.0f * (MAX_MINUTES - runtime_seconds / 60.0f) / MAX_MINUTES;
  }
};

TEST_F(FilterLifetimeTest, PublishesFullLifeAtSetup) {
  this->filter_.setup();
  EXPECT_EQ(this->publications_, 1u);
  EXPECT_FLOAT_EQ(this->last_state_, 100.0f);
  EXPECT_FLOAT_EQ(this->runtime_hours_.state, 0.0f);
  EXPECT_FLOAT_EQ(this->remaining_days_.state, MAX_MINUTES / (24.0f * 60.0f));
}

TEST_F(FilterLifetimeTest, AccumulatesSpeedWeightedSeconds) {
  this->filter_.setup();
  g_speed = 50.0f;
  this->elapse(60000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 30u);
  EXPECT_FLOAT_EQ(this->last_state_, expected_percent(30));
  EXPECT_FLOAT_EQ(this->runtime_hours_.state, 30.0f / 3600.0f);
  EXPECT_FLOAT_EQ(this->remaining_days_.state, (MAX_MINUTES - 0.5f) / (24.0f * 60.0f));
}

TEST_F(FilterLifetimeTest, CarriesSubSecondRemainderAcrossUpdates) {
  this->filter_.setup();
  g_speed = 25.0f;
  for (int i = 0; i < 3; i++)
    this->elapse(1000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 0u);
  this->elapse(1000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 1u);
}

TEST_F(FilterLifetimeTest, NothingAccumulatesWhileOff) {
  this->filter_.setup();
  g_is_on = false;
  this->elapse(60000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 0u);
  EXPECT_EQ(this->publications_, 2u);
  EXPECT_FLOAT_EQ(this->last_state_, 100.0f);
}

TEST_F(FilterLifetimeTest, SpeedIsClampedAndNanCountsAsZero) {
  this->filter_.setup();
  g_speed = 250.0f;
  this->elapse(1000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 1u);
  g_speed = NAN;
  this->elapse(1000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 1u);
  g_speed = -5.0f;
  this->elapse(1000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 1u);
}

TEST_F(FilterLifetimeTest, SpeedSensorIsUsedOnceItHasAState) {
  sensor::Sensor speed;
  this->filter_.set_current_speed_lambda(nullptr);
  this->filter_.set_current_speed_sensor(&speed);
  this->filter_.setup();
  this->elapse(1000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 0u);
  speed.state = 50.0f;
  this->elapse(2000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 1u);
}

TEST_F(FilterLifetimeTest, IsOnSensorGatesAccumulationOnceItIsOn) {
  binary_sensor::BinarySensor running;
  this->filter_.set_is_on_lambda(nullptr);
  this->filter_.set_is_on_sensor(&running);
  this->filter_.setup();
  this->elapse(1000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 0u);
  running.publish_state(true);
  this->elapse(1000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 1u);
  running.publish_state(false);
  this->elapse(1000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 1u);
}

TEST_F(FilterLifetimeTest, NoInputsMeansAlwaysOnAtFullSpeed) {
  this->filter_.set_is_on_lambda(nullptr);
  this->filter_.set_current_speed_lambda(nullptr);
  this->filter_.setup();
  this->elapse(1000);
  EXPECT_EQ(this->filter_.runtime_seconds_, 1u);
}

TEST_F(FilterLifetimeTest, SavesOnlyWhenAnHourOfRuntimeCompletes) {
  this->filter_.setup();
  uint32_t value = 0;
  this->elapse(3599000);
  EXPECT_FALSE(this->stored(value));
  this->elapse(1000);
  ASSERT_TRUE(this->stored(value));
  EXPECT_EQ(value, 3600u);
  this->elapse(1000);
  ASSERT_TRUE(this->stored(value));
  EXPECT_EQ(value, 3600u);
}

TEST_F(FilterLifetimeTest, ShutdownSavesThePartialHour) {
  this->filter_.setup();
  this->elapse(1000);
  this->filter_.on_shutdown();
  uint32_t value = 0;
  ASSERT_TRUE(this->stored(value));
  EXPECT_EQ(value, 1u);
}

TEST_F(FilterLifetimeTest, RestoresRuntimeAtSetup) {
  this->filter_.setup();
  this->elapse(1234000);
  this->filter_.on_shutdown();

  TestableFilterLifetime restored;
  this->attach(restored);
  restored.setup();
  EXPECT_EQ(restored.runtime_seconds_, 1234u);
  EXPECT_FLOAT_EQ(restored.state, expected_percent(1234));
}

TEST_F(FilterLifetimeTest, ResetClearsRuntimeAndSaves) {
  this->filter_.setup();
  this->elapse(5000);
  this->filter_.reset_filter();
  EXPECT_EQ(this->filter_.runtime_seconds_, 0u);
  EXPECT_FLOAT_EQ(this->last_state_, 100.0f);
  EXPECT_FLOAT_EQ(this->runtime_hours_.state, 0.0f);
  uint32_t value = 1;
  ASSERT_TRUE(this->stored(value));
  EXPECT_EQ(value, 0u);
}

TEST_F(FilterLifetimeTest, RemainingLifeStopsAtZero) {
  this->filter_.set_max_lifetime(1);
  this->filter_.setup();
  this->elapse(120000);
  EXPECT_FLOAT_EQ(this->last_state_, 0.0f);
  EXPECT_FLOAT_EQ(this->remaining_days_.state, 0.0f);
  EXPECT_FLOAT_EQ(this->runtime_hours_.state, 2.0f / 60.0f);
}

}  // namespace esphome::filter_lifetime::testing

#endif  // USE_HOST
