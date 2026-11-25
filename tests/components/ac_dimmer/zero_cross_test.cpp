#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>

#include "esphome/components/ac_dimmer/ac_dimmer.h"

namespace esphome::ac_dimmer::testing {

/// Feeds zero-cross edges in time order and runs the timer whenever it asked to be called, as on the ESP8266:
/// each edge re-arms the timer 2 µs later, and a timer with nothing pending waits 20 ms
class Bench {
 public:
  Bench(AcDimmerDataStore &store, uint32_t start) : store_(store), timer_at_(start) {}

  void edge(uint32_t at) {
    this->run_until(at);
    this->store_.zero_cross_edge(at);
    this->timer_at_ = at + 2;
  }

  void run_until(uint32_t at) {
    while (static_cast<int32_t>(at - this->timer_at_) >= 0) {
      const uint32_t now = this->timer_at_;
      const uint32_t next = this->store_.timer_intr(now);
      this->timer_at_ = now + (next != 0 ? next : 20000);
    }
  }

 protected:
  AcDimmerDataStore &store_;
  uint32_t timer_at_;
};

/// Edges from the third crossing on lock the timing; the cases change what follows
static constexpr int LOCK_EDGE = 2;

struct PhaseLockCase {
  const char *name;
  uint32_t start;                 // time of the first crossing
  uint32_t period;                // real half cycle
  int count;                      // crossings
  int32_t jitter;                 // edges after the lock alternate this much late and early
  int32_t noise;                  // after the lock, an extra edge this far from every crossing (0 for none)
  int drop_from;                  // first crossing without an edge (0 for none)
  int drop_count;                 // crossings in a row without an edge
  int32_t tolerance;              // allowed distance of the predicted crossing from the real one
  bool jitter_from_start{false};  // the jitter also applies while locking
};

class PhaseLockTest : public ::testing::TestWithParam<PhaseLockCase> {};

TEST_P(PhaseLockTest, FollowsCrossings) {
  const auto &c = GetParam();
  AcDimmerDataStore store{};
  store.value = 32768;
  Bench bench(store, c.start);
  uint32_t crossing = c.start;
  for (int i = 0; i < c.count; i++, crossing += c.period) {
    const bool tracking = i > LOCK_EDGE;
    if (tracking && c.noise < 0)
      bench.edge(crossing + c.noise);
    if (i < c.drop_from || i >= c.drop_from + c.drop_count) {
      const int32_t jitter = tracking || c.jitter_from_start ? (i % 2 ? c.jitter : -c.jitter) : 0;
      bench.edge(crossing + jitter);
    }
    if (tracking && c.noise > 0)
      bench.edge(crossing + c.noise);
    if (i == LOCK_EDGE)
      EXPECT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED);
  }
  const uint32_t last = crossing - c.period;
  bench.run_until(last + c.period / 2);
  EXPECT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED);
  EXPECT_LE(std::abs(static_cast<int32_t>(store.zc_at - last)), c.tolerance);
  EXPECT_LE(std::abs(static_cast<int32_t>(store.period - c.period)), 20);
}

INSTANTIATE_TEST_SUITE_P(
    AcDimmer, PhaseLockTest,
    ::testing::Values(PhaseLockCase{"Clean50Hz", 1000, 10000, 50, 0, 0, 0, 0, 2},
                      PhaseLockCase{"Clean60Hz", 1000, 8333, 50, 0, 0, 0, 0, 2},
                      // micros() wraps around, and one crossing is exactly at 0
                      PhaseLockCase{"CrossingAtZeroMicros", 0U - 30000U, 10000, 50, 0, 0, 0, 0, 2},
                      PhaseLockCase{"AlternatingJitter", 1000, 10000, 200, 250, 0, 0, 0, 200},
                      PhaseLockCase{"AlternatingJitterWhileLocking", 1000, 10000, 200, 250, 0, 0, 0, 200, true},
                      // Noise half way through every half cycle (the triac firing) is outside the window
                      PhaseLockCase{"NoiseOutsideWindow", 1000, 10000, 200, 0, 5000, 0, 0, 2},
                      // Noise 1 ms before every crossing is inside the window: the edge closest to the
                      // prediction is kept, so the noise does not drag the timing
                      PhaseLockCase{"NoiseJustBeforeCrossing", 1000, 10000, 200, 0, -1000, 0, 0, 2},
                      PhaseLockCase{"MissedCrossing", 1000, 10000, 50, 0, 0, 20, 1, 2},
                      PhaseLockCase{"SevenMissedCrossings", 1000, 10000, 50, 0, 0, 20, 7, 2}),
    [](const ::testing::TestParamInfo<PhaseLockCase> &info) { return info.param.name; });

/// Locks a dimmer on crossings at 1, 11 and 21 ms; the caller runs the timer at 21 ms to start that half cycle
static AcDimmerDataStore locked_store(DimMethod method, uint16_t value) {
  AcDimmerDataStore store{};
  store.method = method;
  store.value = value;
  for (uint32_t at : {1000U, 11000U, 21000U})
    store.zero_cross_edge(at);
  return store;
}

TEST(AcDimmerTimerTest, NothingBeforeTheLock) {
  AcDimmerDataStore store{};
  store.value = 32768;
  for (uint32_t at : {1000U, 11000U}) {
    store.zero_cross_edge(at);
    EXPECT_NE(store.phase, ZERO_CROSS_PHASE_LOCKED);
    EXPECT_EQ(store.timer_intr(at + 100), 0U);
    EXPECT_EQ(store.enable_time_us, 0U);
  }
}

// A gap longer than any half cycle while locking starts the measurement over
TEST(AcDimmerTimerTest, LongGapStartsTheLockOver) {
  AcDimmerDataStore store{};
  for (uint32_t at : {1000U, 11000U, 41000U, 51000U}) {
    store.zero_cross_edge(at);
    EXPECT_NE(store.phase, ZERO_CROSS_PHASE_LOCKED);
  }
  store.zero_cross_edge(61000);
  EXPECT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED);
  EXPECT_EQ(store.period, 10000);
}

// A crossing has no edge for eight half cycles in a row: the lock is dropped and the timer goes idle
TEST(AcDimmerTimerTest, EightMissesDropTheLock) {
  auto store = locked_store(DIM_METHOD_LEADING_PULSE, 32768);
  store.timer_intr(21000);
  store.timer_intr(31000);  // the crossing at 21 ms had its edge
  for (uint32_t at = 41000; at < 111000; at += 10000) {
    EXPECT_NE(store.timer_intr(at), 0U);
    EXPECT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED);
    EXPECT_EQ(store.zc_at, at);
  }
  EXPECT_EQ(store.timer_intr(111000), 0U);
  EXPECT_EQ(store.phase, ZERO_CROSS_PHASE_NONE);
  EXPECT_EQ(store.timer_intr(111100), 0U);
}

struct ScheduleCase {
  const char *name;
  DimMethod method;
  uint16_t value;
  bool init_cycle;
  uint16_t enable_time_us;
  uint16_t disable_time_us;
  uint32_t first_event_in;
};

class ScheduleTest : public ::testing::TestWithParam<ScheduleCase> {};

TEST_P(ScheduleTest, StartsHalfCycleAtTheCrossing) {
  const auto &c = GetParam();
  auto store = locked_store(c.method, c.value);
  store.init_cycle = c.init_cycle;
  EXPECT_EQ(store.timer_intr(21000), c.first_event_in);
  EXPECT_EQ(store.zc_at, 21000U);
  EXPECT_EQ(store.enable_time_us, c.enable_time_us);
  EXPECT_EQ(store.disable_time_us, c.disable_time_us);
}

INSTANTIATE_TEST_SUITE_P(
    AcDimmer, ScheduleTest,
    ::testing::Values(ScheduleCase{"LeadingPulse", DIM_METHOD_LEADING_PULSE, 32768, false, 4999, 5049, 4999},
                      ScheduleCase{"Leading", DIM_METHOD_LEADING, 32768, false, 4999, 10000, 4999},
                      ScheduleCase{"Trailing", DIM_METHOD_TRAILING, 32768, false, 1, 5000, 1},
                      ScheduleCase{"InitCycle", DIM_METHOD_LEADING_PULSE, 32768, true, 0, 10000, 10000}),
    [](const ::testing::TestParamInfo<ScheduleCase> &info) { return info.param.name; });

struct LateEnableCase {
  const char *name;
  uint32_t timer_at;  // time after the crossing the timer runs for the enable due at 9600 µs
  uint16_t disable_time_us;
  uint32_t next_event_in;
};

class LateEnableTest : public ::testing::TestWithParam<LateEnableCase> {};

// A timer delayed by masked sections must not switch the gate on so close to the end of the half cycle that the
// triac stays on into the next one
TEST_P(LateEnableTest, SkipsEnableNearTheEnd) {
  const auto &c = GetParam();
  auto store = locked_store(DIM_METHOD_LEADING_PULSE, 2621);
  ASSERT_EQ(store.timer_intr(21000), 9600U);
  EXPECT_EQ(store.timer_intr(21000 + c.timer_at), c.next_event_in);
  EXPECT_EQ(store.enable_time_us, 0U);
  EXPECT_EQ(store.disable_time_us, c.disable_time_us);
}

INSTANTIATE_TEST_SUITE_P(AcDimmer, LateEnableTest,
                         ::testing::Values(LateEnableCase{"OnTime", 9600, 9650, 50},
                                           LateEnableCase{"LateOutsideGuard", 9700, 9750, 50},
                                           // not switched on; the pending switch-off runs at once
                                           LateEnableCase{"LateInsideGuard", 9850, 0, 150}),
                         [](const ::testing::TestParamInfo<LateEnableCase> &info) { return info.param.name; });

}  // namespace esphome::ac_dimmer::testing
