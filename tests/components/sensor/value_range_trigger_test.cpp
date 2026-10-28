#ifdef USE_HOST
#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

#include "esphome/components/host/preferences.h"
#include "esphome/components/sensor/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/base_automation.h"

namespace esphome::sensor::testing {
namespace fs = std::filesystem;

class TestSensor : public Sensor {
 public:
  void set_test_id(uint32_t hash) { this->object_id_hash_ = hash; }
};

/// A sensor, its on_value_range trigger and the values the trigger fired with
struct RangeFixture {
  TestSensor sensor;
  ValueRangeTrigger trigger{&this->sensor};
  Automation<float> automation{&this->trigger};
  std::vector<float> fired;
  LambdaAction<float> record{[this](float x) { this->fired.push_back(x); }};

  explicit RangeFixture(uint32_t id = 0x5E01) {
    this->sensor.set_test_id(id);
    this->automation.add_action(&this->record);
  }
  void publish(std::initializer_list<float> values) {
    for (float value : values)
      this->sensor.publish_state(value);
  }
};

class SensorValueRangeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = fs::temp_directory_path() / "esphome_sensor_value_range_test";
    fs::remove_all(this->temp_dir_);
    fs::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    App.pre_setup("test_sensor_value_range", strlen("test_sensor_value_range"), "", 0);
    host::setup_preferences();
    host::host_preferences->reset();
  }

  void TearDown() override {
    unsetenv("ESPHOME_PREFDIR");
    fs::remove_all(this->temp_dir_);
  }

  fs::path temp_dir_;
};

TEST_F(SensorValueRangeTest, AboveOnlyFiresOnEnteringTheRange) {
  RangeFixture f;
  f.trigger.set_min([](float) -> float { return 10.0f; });
  f.trigger.setup();
  f.publish({5.0f, 10.0f, 20.0f, 9.9f, 12.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{10.0f, 12.0f}));
}

TEST_F(SensorValueRangeTest, BelowOnlyFiresOnEnteringTheRange) {
  RangeFixture f;
  f.trigger.set_max([](float) -> float { return 10.0f; });
  f.trigger.setup();
  f.publish({15.0f, 10.0f, 5.0f, 10.1f, 8.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{10.0f, 8.0f}));
}

TEST_F(SensorValueRangeTest, BothBoundsAreInclusive) {
  RangeFixture f;
  f.trigger.set_min([](float) -> float { return 10.0f; });
  f.trigger.set_max([](float) -> float { return 20.0f; });
  f.trigger.setup();
  f.publish({9.0f, 10.0f, 20.0f, 21.0f, 20.0f, 25.0f, 15.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{10.0f, 20.0f, 15.0f}));
}

TEST_F(SensorValueRangeTest, NanStateIsIgnored) {
  RangeFixture f;
  f.trigger.set_min([](float) -> float { return 10.0f; });
  f.trigger.setup();
  f.publish({15.0f, NAN, 16.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{15.0f}));
}

// Bounds get the state as x; a NaN bound is ignored, and with both NaN the previous result is kept
TEST_F(SensorValueRangeTest, NanBoundsFromLambdas) {
  RangeFixture f;
  f.trigger.set_min([](float x) -> float { return x > 100.0f ? NAN : 10.0f; });
  f.trigger.set_max([](float x) -> float { return x > 100.0f || x < 0.0f ? NAN : 20.0f; });
  f.trigger.setup();
  // 200: both NaN keeps "in range"; -5: only the max is NaN, so -5 >= 10 is out of range
  f.publish({15.0f, 200.0f, 16.0f, -5.0f, 200.0f, 17.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{15.0f, 17.0f}));
}

TEST_F(SensorValueRangeTest, InRangeIsRestored) {
  {
    RangeFixture f;
    f.trigger.set_min([](float) -> float { return 10.0f; });
    f.trigger.setup();
    f.publish({15.0f});
    ASSERT_EQ(f.fired.size(), 1u);
    global_preferences->sync();
  }
  RangeFixture f;
  f.trigger.set_min([](float) -> float { return 10.0f; });
  f.trigger.setup();
  f.publish({16.0f});
  EXPECT_TRUE(f.fired.empty()) << "the stored in-range state was not restored";
  bool stored = false;
  EXPECT_TRUE(f.sensor.make_entity_preference<bool>().load(&stored));
  EXPECT_TRUE(stored);
}

}  // namespace esphome::sensor::testing
#endif  // USE_HOST
