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
struct Fixture {
  sensor::Sensor source;
  DerivativeSensor derivative;
  int publishes{0};
  float last_rate{NAN};

  explicit Fixture(float time_factor) : derivative(&this->source, time_factor) {
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

}  // namespace

// The same 2.0 change over 1000 ms is scaled by the multiplier generated for each time unit.
TEST(DerivativeSensor, RateIsScaledToTheConfiguredTimeUnit) {
  struct Case {
    float time_factor;
    float expected;
  };
  const Case cases[] = {
      {1.0f, 0.002f},                   // ms: 2.0 / 1000 ms
      {1.0f / 1000.0f, 2.0f},           // s:  2.0 / 1 s
      {1.0f / 60000.0f, 120.0f},        // min: 2.0 / (1/60 min)
      {1.0f / 3600000.0f, 7200.0f},     // h:  2.0 / (1/3600 h)
      {1.0f / 86400000.0f, 172800.0f},  // d:  2.0 / (1/86400 d)
  };
  for (const Case &c : cases) {
    Fixture f(c.time_factor);
    EXPECT_FALSE(f.feed(1000, 10.0f)) << "baseline published a rate";
    EXPECT_TRUE(f.feed(2000, 12.0f));
    EXPECT_NEAR(f.last_rate, c.expected, c.expected * 1e-4f);
  }
}

// A NaN reading drops the baseline: nothing is published across the gap, and the first valid
// reading afterwards only re-establishes it, so the gap never produces a bogus spike.
TEST(DerivativeSensor, NaNResetsTheBaselineAndTheNextSampleOnlyReestablishesIt) {
  Fixture f(1.0f / 1000.0f);
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
  Fixture f(1.0f / 1000.0f);
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
  Fixture f(1.0f / 1000.0f);
  EXPECT_FALSE(f.feed(0xFFFFFF00u, 10.0f));  // baseline just before the wrap
  EXPECT_TRUE(f.feed(0x00000100u, 15.12f));  // 512 ms = 0.512 s later
  EXPECT_NEAR(f.last_rate, 10.0f, 1e-4f);    // (15.12 - 10) / 0.512
}

}  // namespace esphome::derivative::testing
