#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "esphome/components/derivative/derivative_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome::derivative::testing {

namespace {

// Sets the loop clock the component reads through App.get_loop_component_start_time(), the
// same value a synchronous source publish sees in a real loop. The guard's destructor does
// not restore the previous value, so the clock stays where the last call left it.
void set_loop_time(uint32_t now) { LoopBlockingGuard clock(nullptr, LOG_STR("test"), now); }

// Drives a DerivativeSensor from a real source sensor, publishing values at controlled loop
// times exactly as the generated code does: the source publish reaches process_sensor_value_
// synchronously while the loop clock holds the value set for that step.
struct Fixture {
  sensor::Sensor source;
  DerivativeSensor derivative;
  int publishes{0};
  float last_rate{NAN};

  Fixture(DerivativeSensorTime unit, uint32_t start_time) {
    this->derivative.set_source_sensor(&this->source);
    this->derivative.set_time(unit);
    this->derivative.add_on_state_callback([this](float rate) {
      this->publishes++;
      this->last_rate = rate;
    });
    // setup() takes its initial timestamp from the loop clock, so set it first.
    set_loop_time(start_time);
    this->derivative.setup();
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

// The same 2.0 change over 1000 ms is scaled by the configured time unit.
TEST(DerivativeTimeUnit, RateIsScaledToTheConfiguredTimeUnit) {
  struct Case {
    DerivativeSensorTime unit;
    float expected;
  };
  const Case cases[] = {
      {DERIVATIVE_SENSOR_TIME_MILLISECOND, 0.002f},  // 2.0 / 1000 ms
      {DERIVATIVE_SENSOR_TIME_SECOND, 2.0f},         // 2.0 / 1 s
      {DERIVATIVE_SENSOR_TIME_MINUTE, 120.0f},       // 2.0 / (1/60 min)
      {DERIVATIVE_SENSOR_TIME_HOUR, 7200.0f},        // 2.0 / (1/3600 h)
      {DERIVATIVE_SENSOR_TIME_DAY, 172800.0f},       // 2.0 / (1/86400 d)
  };
  for (const Case &c : cases) {
    Fixture f(c.unit, 1000);
    EXPECT_FALSE(f.feed(1000, 10.0f)) << "baseline published a rate";
    EXPECT_TRUE(f.feed(2000, 12.0f));
    EXPECT_NEAR(f.last_rate, c.expected, c.expected * 1e-4f);
  }
}

// The first reading ever only establishes the baseline; nothing is published for it.
TEST(DerivativeBaseline, FirstSampleOnlyEstablishesTheBaseline) {
  Fixture f(DERIVATIVE_SENSOR_TIME_SECOND, 1000);
  EXPECT_FALSE(f.derivative.has_state());

  EXPECT_FALSE(f.feed(1000, 10.0f));
  EXPECT_EQ(f.publishes, 0);
  EXPECT_FALSE(f.derivative.has_state());

  // The next reading derives from the baseline.
  EXPECT_TRUE(f.feed(2000, 12.0f));
  EXPECT_FLOAT_EQ(f.last_rate, 2.0f);
}

// A NaN reading drops the baseline: nothing is published across the gap, and the first valid
// reading afterwards only re-establishes it, so the gap never produces a bogus spike.
TEST(DerivativeNanRecovery, NaNResetsTheBaselineAndTheNextSampleOnlyReestablishesIt) {
  Fixture f(DERIVATIVE_SENSOR_TIME_SECOND, 1000);
  EXPECT_FALSE(f.feed(1000, 10.0f));  // baseline
  EXPECT_FALSE(f.feed(2000, NAN));    // gap: baseline dropped, no publish
  EXPECT_EQ(f.publishes, 0);

  EXPECT_FALSE(f.feed(3000, 20.0f));  // first valid after the gap: baseline only
  EXPECT_EQ(f.publishes, 0);

  EXPECT_TRUE(f.feed(4000, 22.0f));  // (22 - 20) over 1 s
  EXPECT_FLOAT_EQ(f.last_rate, 2.0f);
}

// Two readings in the same loop iteration have no elapsed time: nothing is published and the
// (guarded) dt is never used as a divisor.
TEST(DerivativeZeroElapsed, SameLoopTimePublishesNothing) {
  Fixture f(DERIVATIVE_SENSOR_TIME_SECOND, 1000);
  EXPECT_FALSE(f.feed(1000, 10.0f));  // baseline at t = 1000
  EXPECT_FALSE(f.feed(1000, 15.0f));  // dt == 0
  EXPECT_EQ(f.publishes, 0);
  EXPECT_FALSE(f.derivative.has_state());

  // The same-instant reading becomes the new baseline: (17 - 15) over 1 s.
  EXPECT_TRUE(f.feed(2000, 17.0f));
  EXPECT_FLOAT_EQ(f.last_rate, 2.0f);
}

// The loop clock is a uint32_t and wraps; the delta must be the unsigned difference, not a
// huge negative one.
TEST(DerivativeRollover, LoopClockWraparoundUsesTheUnsignedDelta) {
  // 0xFFFFFF00 -> 0x00000100 wraps around: 512 ms elapsed, not a ~49 day negative gap.
  Fixture f(DERIVATIVE_SENSOR_TIME_SECOND, 0xFFFFFF00u);
  EXPECT_FALSE(f.feed(0xFFFFFF00u, 10.0f));  // baseline just before the wrap
  EXPECT_TRUE(f.feed(0x00000100u, 15.12f));  // 512 ms = 0.512 s later
  EXPECT_NEAR(f.last_rate, 10.0f, 1e-4f);    // (15.12 - 10) / 0.512
}

}  // namespace esphome::derivative::testing
