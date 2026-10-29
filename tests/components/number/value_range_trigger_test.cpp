#ifdef USE_HOST
#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

#include "esphome/components/host/preferences.h"
#include "esphome/components/number/automation.h"
#include "esphome/components/number/number.h"
#include "esphome/core/application.h"
#include "esphome/core/base_automation.h"

namespace esphome::number::testing {
namespace fs = std::filesystem;

class TestNumber : public Number {
 public:
  void control(float value) override {}
  void set_test_id(uint32_t hash) { this->object_id_hash_ = hash; }
};

/// A number, its on_value_range trigger and the values the trigger fired with
struct RangeFixture {
  TestNumber number;
  ValueRangeTrigger trigger{&this->number};
  Automation<float> automation{&this->trigger};
  std::vector<float> fired;
  LambdaAction<float> record{[this](float x) { this->fired.push_back(x); }};

  RangeFixture() {
    this->number.set_test_id(0x5E01);
    this->automation.add_action(&this->record);
  }
  void publish(std::initializer_list<float> values) {
    for (float value : values)
      this->number.publish_state(value);
  }
};

class NumberValueRangeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->temp_dir_ = fs::temp_directory_path() / "esphome_number_value_range_test";
    fs::remove_all(this->temp_dir_);
    fs::create_directories(this->temp_dir_);
    setenv("ESPHOME_PREFDIR", this->temp_dir_.c_str(), 1);
    App.pre_setup("test_number_value_range", strlen("test_number_value_range"), "", 0);
    host::setup_preferences();
    host::host_preferences->reset();
  }

  void TearDown() override {
    unsetenv("ESPHOME_PREFDIR");
    fs::remove_all(this->temp_dir_);
  }

  fs::path temp_dir_;
};

TEST_F(NumberValueRangeTest, AboveOnlyFiresOnEnteringTheRange) {
  RangeFixture f;
  f.trigger.set_min([](float) -> float { return 10.0f; });
  f.trigger.setup();
  f.publish({5.0f, 10.0f, 20.0f, 9.9f, 12.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{10.0f, 12.0f}));
}

TEST_F(NumberValueRangeTest, BelowOnlyFiresOnEnteringTheRange) {
  RangeFixture f;
  f.trigger.set_max([](float) -> float { return 10.0f; });
  f.trigger.setup();
  f.publish({15.0f, 10.0f, 5.0f, 10.1f, 8.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{10.0f, 8.0f}));
}

TEST_F(NumberValueRangeTest, BothBoundsAreInclusive) {
  RangeFixture f;
  f.trigger.set_min([](float) -> float { return 10.0f; });
  f.trigger.set_max([](float) -> float { return 20.0f; });
  f.trigger.setup();
  f.publish({9.0f, 10.0f, 20.0f, 21.0f, 20.0f, 25.0f, 15.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{10.0f, 20.0f, 15.0f}));
}

TEST_F(NumberValueRangeTest, NanStateIsIgnored) {
  RangeFixture f;
  f.trigger.set_min([](float) -> float { return 10.0f; });
  f.trigger.setup();
  f.publish({15.0f, NAN, 16.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{15.0f}));
}

// Bounds get the state as x; a NaN bound is ignored, and with both NaN the previous result is kept
TEST_F(NumberValueRangeTest, NanBoundsFromLambdas) {
  RangeFixture f;
  f.trigger.set_min([](float x) -> float { return x > 100.0f ? NAN : 10.0f; });
  f.trigger.set_max([](float x) -> float { return x > 100.0f || x < 0.0f ? NAN : 20.0f; });
  f.trigger.setup();
  // 200: both NaN keeps "in range"; -5: only the max is NaN, so -5 >= 10 is out of range
  f.publish({15.0f, 200.0f, 16.0f, -5.0f, 200.0f, 17.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{15.0f, 17.0f}));
}

// The preference is keyed by the range width (max - min with x = 0), so a changed range starts over
TEST_F(NumberValueRangeTest, InRangeIsRestoredForTheSameWidth) {
  {
    RangeFixture f;
    f.trigger.set_min([](float) -> float { return 10.0f; });
    f.trigger.set_max([](float x) -> float { return x + 20.0f; });
    f.trigger.setup();
    f.publish({15.0f});
    ASSERT_EQ(f.fired.size(), 1u);
    global_preferences->sync();
  }
  {
    RangeFixture f;
    f.trigger.set_min([](float) -> float { return 10.0f; });
    f.trigger.set_max([](float x) -> float { return x + 20.0f; });
    f.trigger.setup();
    f.publish({16.0f});
    EXPECT_TRUE(f.fired.empty()) << "the stored in-range state was not restored";
    float width = 10.0f;
    uint32_t hash;
    memcpy(&hash, &width, sizeof(hash));
    bool stored = false;
    EXPECT_TRUE(f.number.make_entity_preference<bool>(hash).load(&stored));
    EXPECT_TRUE(stored);
  }
  RangeFixture f;
  f.trigger.set_min([](float) -> float { return 10.0f; });
  f.trigger.set_max([](float) -> float { return 30.0f; });
  f.trigger.setup();
  f.publish({16.0f});
  EXPECT_EQ(f.fired, (std::vector<float>{16.0f}));
}

}  // namespace esphome::number::testing
#endif  // USE_HOST
