#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <span>

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

template<size_t N> struct WeightedMeanFixture {
  std::array<sensor::Sensor, N> sources;
  alignas(WeightedMeanCombinationComponent::SensorSource)
      std::array<unsigned char, sizeof(WeightedMeanCombinationComponent::SensorSource) * N> storage{};
  WeightedMeanCombinationComponent combination;

  WeightedMeanFixture(std::initializer_list<float> states, std::initializer_list<float> weights) {
    this->combination.set_source_storage(std::span<WeightedMeanCombinationComponent::SensorSource>(
        reinterpret_cast<WeightedMeanCombinationComponent::SensorSource *>(this->storage.data()), N));
    size_t i = 0;
    const auto *weight = weights.begin();
    for (float state : states) {
      this->sources[i].state = state;
      this->combination.add_source(&this->sources[i++], *weight++);
    }
  }

  WeightedMeanFixture(std::initializer_list<float> states, std::initializer_list<std::function<float(float)>> weights) {
    this->combination.set_source_storage(std::span<WeightedMeanCombinationComponent::SensorSource>(
        reinterpret_cast<WeightedMeanCombinationComponent::SensorSource *>(this->storage.data()), N));
    size_t i = 0;
    const auto *weight = weights.begin();
    for (float state : states) {
      this->sources[i].state = state;
      this->combination.add_source(&this->sources[i++], *weight++);
    }
  }

  float run() { return this->run_with_trigger(0.0f); }

  float run_with_trigger(float trigger) {
    this->combination.handle_new_value(trigger);
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

TEST(WeightedMeanCombination, CalculatesWeightedMean) {
  WeightedMeanFixture<2> f({10.0f, 20.0f}, {1.0f, 3.0f});
  EXPECT_FLOAT_EQ(f.run(), 17.5f);
}

TEST(WeightedMeanCombination, SkipsNonFiniteStatesAndNonPositiveWeights) {
  WeightedMeanFixture<3> f({10.0f, NAN, 20.0f}, {0.0f, 2.0f, 1.0f});
  EXPECT_FLOAT_EQ(f.run(), 20.0f);
}

TEST(WeightedMeanCombination, SkipsNonFiniteWeights) {
  WeightedMeanFixture<3> f({10.0f, 20.0f, 30.0f}, {NAN, INFINITY, 1.0f});
  EXPECT_FLOAT_EQ(f.run(), 30.0f);
}

TEST(WeightedMeanCombination, AvoidsFloatOverflow) {
  WeightedMeanFixture<2> f({10.0f, 20.0f}, {1.0e38f, 1.0e38f});
  EXPECT_FLOAT_EQ(f.run(), 15.0f);
}

TEST(WeightedMeanCombination, NoPositiveWeightIsNan) {
  WeightedMeanFixture<2> f({10.0f, 20.0f}, {0.0f, 0.0f});
  EXPECT_TRUE(std::isnan(f.run()));
}

TEST(WeightedMeanCombination, RecomputesForNanTrigger) {
  WeightedMeanFixture<2> f({NAN, 20.0f}, {1.0f, 1.0f});
  EXPECT_FLOAT_EQ(f.run_with_trigger(NAN), 20.0f);
}

TEST(WeightedMeanCombination, RecomputesLambdaWeightsWithSourceState) {
  WeightedMeanFixture<2> f({10.0f, 20.0f}, {[](float x) -> float { return x; }, [](float) -> float { return 2.0f; }});
  EXPECT_FLOAT_EQ(f.run(), 11.666666f);

  f.sources[0].state = 30.0f;
  EXPECT_FLOAT_EQ(f.run(), 29.375f);
}

}  // namespace esphome::combination::testing
