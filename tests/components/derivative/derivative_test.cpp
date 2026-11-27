#include <gtest/gtest.h>

#include <cmath>

#include "esphome/components/derivative/derivative_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/application.h"

namespace esphome::derivative::testing {

namespace {

// Sets App.get_loop_component_start_time(); the guard leaves it set when it goes out of scope.
void set_loop_time(uint32_t now) { LoopBlockingGuard clock(nullptr, LOG_STR("test"), now); }

// Publishes source readings at chosen loop times, as the generated code does.
template<uint32_t MS_PER_UNIT = 1000> struct Fixture {
  sensor::Sensor source;
  DerivativeSensor<MS_PER_UNIT> derivative;
  int publishes{0};
  float last_rate{NAN};

  Fixture() : derivative(&this->source) {
    this->derivative.add_on_state_callback([this](float rate) {
      this->publishes++;
      this->last_rate = rate;
    });
  }

  // Publishes one source reading at the given loop time; returns true when the derivative
  // published a rate for that reading.
  bool feed(uint32_t now, float value) {
    const int before = this->publishes;
    set_loop_time(now);
    this->source.publish_state(value);
    return this->publishes > before;
  }
};

// A 2.0 change over 1000 ms, scaled to the time unit MS_PER_UNIT.
template<uint32_t MS_PER_UNIT> void expect_rate(float expected) {
  Fixture<MS_PER_UNIT> f;
  EXPECT_FALSE(f.feed(1000, 10.0f)) << "baseline published a rate";
  EXPECT_TRUE(f.feed(2000, 12.0f));
  EXPECT_NEAR(f.last_rate, expected, expected * 1e-4f);
}

}  // namespace

TEST(DerivativeSensor, RateIsScaledToTheConfiguredTimeUnit) {
  expect_rate<1>(0.002f);            // ms
  expect_rate<1000>(2.0f);           // s
  expect_rate<60000>(120.0f);        // min
  expect_rate<3600000>(7200.0f);     // h
  expect_rate<86400000>(172800.0f);  // d
}

// A NaN reading drops the baseline: nothing is published across the gap, and the first valid
// reading afterwards only re-establishes it, so the gap never produces a bogus spike.
TEST(DerivativeSensor, NaNResetsTheBaselineAndTheNextSampleOnlyReestablishesIt) {
  Fixture<> f;
  EXPECT_FALSE(f.feed(1000, 10.0f));  // baseline
  EXPECT_FALSE(f.feed(2000, NAN));    // gap: baseline dropped, no publish
  EXPECT_EQ(f.publishes, 0);

  EXPECT_FALSE(f.feed(3000, 20.0f));  // first valid after the gap: baseline only
  EXPECT_EQ(f.publishes, 0);

  EXPECT_TRUE(f.feed(4000, 22.0f));  // (22 - 20) over 1 s
  EXPECT_FLOAT_EQ(f.last_rate, 2.0f);
}

// Two readings at the same loop time publish nothing, and the older baseline is kept so the
// change shows up on the next reading.
TEST(DerivativeSensor, SameLoopTimeKeepsTheOlderBaseline) {
  Fixture<> f;
  EXPECT_FALSE(f.feed(1000, 10.0f));  // baseline at t = 1000
  EXPECT_FALSE(f.feed(1000, 15.0f));  // dt == 0
  EXPECT_EQ(f.publishes, 0);
  EXPECT_FALSE(f.derivative.has_state());

  // The first baseline is kept, so the same-instant change is not lost: (17 - 10) over 1 s.
  EXPECT_TRUE(f.feed(2000, 17.0f));
  EXPECT_FLOAT_EQ(f.last_rate, 7.0f);
}

// The loop clock is a uint32_t and wraps; the delta must be the unsigned difference, not a
// huge negative one.
TEST(DerivativeSensor, LoopClockWraparoundUsesTheUnsignedDelta) {
  // 0xFFFFFF00 -> 0x00000100 wraps around: 512 ms elapsed, not a ~49 day negative gap.
  Fixture<> f;
  EXPECT_FALSE(f.feed(0xFFFFFF00u, 10.0f));  // baseline just before the wrap
  EXPECT_TRUE(f.feed(0x00000100u, 15.12f));  // 512 ms = 0.512 s later
  EXPECT_NEAR(f.last_rate, 10.0f, 1e-4f);    // (15.12 - 10) / 0.512
}

}  // namespace esphome::derivative::testing
