#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <initializer_list>

#include "esphome/components/combination/combination.h"

namespace esphome::combination::testing {

// Sources with the given states, held in the static storage code generation would emit
template<typename T, size_t N> struct Fixture {
  T combination;
  std::array<sensor::Sensor, N> sources;
  std::array<sensor::Sensor *, N> storage{};

  explicit Fixture(std::initializer_list<float> states) {
    this->combination.set_sensor_storage(this->storage);
    size_t i = 0;
    for (float state : states) {
      this->sources[i].state = state;
      this->combination.add_source(&this->sources[i++]);
    }
  }

  float run() {
    this->combination.handle_new_value(0.0f);
    return this->combination.get_state();
  }
};

TEST(MedianCombination, OddCountTakesTheMiddle) {
  Fixture<MedianCombinationComponent, 3> f({3.0f, 1.0f, 2.0f});
  EXPECT_FLOAT_EQ(f.run(), 2.0f);
}

TEST(MedianCombination, EvenCountAveragesTheMiddleTwo) {
  Fixture<MedianCombinationComponent, 4> f({4.0f, 1.0f, 3.0f, 2.0f});
  EXPECT_FLOAT_EQ(f.run(), 2.5f);
}

TEST(MedianCombination, SkipsNonFiniteStates) {
  Fixture<MedianCombinationComponent, 3> f({NAN, 5.0f, 1.0f});
  EXPECT_FLOAT_EQ(f.run(), 3.0f);
}

TEST(MedianCombination, MoreSourcesThanTheStackBuffer) {
  Fixture<MedianCombinationComponent, 9> f({9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f});
  EXPECT_FLOAT_EQ(f.run(), 5.0f);
}

TEST(MedianCombination, NoFiniteStateIsNan) {
  Fixture<MedianCombinationComponent, 2> f({NAN, INFINITY});
  EXPECT_TRUE(std::isnan(f.run()));
}

TEST(MinMaxCombination, SkipNonFiniteStates) {
  Fixture<MinimumCombinationComponent, 3> min({3.0f, NAN, -2.0f});
  EXPECT_FLOAT_EQ(min.run(), -2.0f);
  Fixture<MaximumCombinationComponent, 3> max({3.0f, NAN, -2.0f});
  EXPECT_FLOAT_EQ(max.run(), 3.0f);
}

TEST(MinMaxCombination, NoFiniteStateIsInfinite) {
  Fixture<MinimumCombinationComponent, 1> min({NAN});
  EXPECT_EQ(min.run(), INFINITY);
  Fixture<MaximumCombinationComponent, 1> max({NAN});
  EXPECT_EQ(max.run(), -INFINITY);
}

TEST(RangeCombination, LargestMinusSmallest) {
  Fixture<RangeCombinationComponent, 4> f({3.0f, -2.0f, NAN, 5.0f});
  EXPECT_FLOAT_EQ(f.run(), 7.0f);
}

TEST(RangeCombination, OneFiniteStateIsZero) {
  Fixture<RangeCombinationComponent, 2> f({NAN, 4.0f});
  EXPECT_FLOAT_EQ(f.run(), 0.0f);
}

TEST(RangeCombination, NoFiniteStateIsNan) {
  Fixture<RangeCombinationComponent, 2> f({NAN, -INFINITY});
  EXPECT_TRUE(std::isnan(f.run()));
}

}  // namespace esphome::combination::testing
