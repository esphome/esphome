#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "esphome/components/ac_dimmer/ac_dimmer.h"

namespace esphome::ac_dimmer::testing {

/// Feeds zero-cross edges in time order and runs the timer whenever it asked to be called, as on the ESP8266:
/// each edge re-arms the timer 2 µs later, and a timer with nothing pending waits 20 ms. With `tick`, the timer
/// runs every `tick` µs instead, as on the ESP32.
class Bench {
 public:
  /// With `gate_noise`, the triac firing disturbs the detector: an extra edge comes that long after the gate goes on
  Bench(AcDimmerDataStore &store, uint32_t start, uint32_t gate_noise = 0, uint32_t tick = 0)
      : store_(store), timer_at_(start), gate_noise_(gate_noise), tick_(tick) {}

  void edge(uint32_t at) {
    this->run_until(at);
    this->store_.zero_cross_edge(at);
    if (this->tick_ == 0)
      this->timer_at_ = at + 2;
  }

  void run_until(uint32_t at) {
    while (static_cast<int32_t>(at - this->timer_at_) >= 0) {
      const uint32_t now = this->timer_at_;
      const bool enable_pending = this->store_.enable_time_us != 0;
      const uint32_t next = this->store_.timer_intr(now);
      this->timer_at_ = now + (this->tick_ != 0 ? this->tick_ : next != 0 ? next : 20000);
      if (!enable_pending || this->store_.enable_time_us != 0)
        continue;
      this->gate_on.push_back(now);
      const uint32_t noise = now + this->gate_noise_;
      if (this->gate_noise_ != 0 && static_cast<int32_t>(at - noise) >= 0) {
        this->store_.zero_cross_edge(noise);
        if (this->tick_ == 0)
          this->timer_at_ = noise + 2;
      }
    }
  }

  /// Times the gate was switched on (the timer always runs on time here, so a due pulse always starts)
  std::vector<uint32_t> gate_on;

 protected:
  AcDimmerDataStore &store_;
  uint32_t timer_at_;
  uint32_t gate_noise_;
  uint32_t tick_;
};

/// Edges from the fifth crossing on lock the timing; the cases change what follows
static constexpr int LOCK_EDGE = 4;

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
  int32_t period_tolerance{20};   // allowed distance of the measured half cycle from the real one
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
  EXPECT_LE(std::abs(static_cast<int32_t>(store.period - c.period)), c.period_tolerance);
}

INSTANTIATE_TEST_SUITE_P(
    AcDimmer, PhaseLockTest,
    ::testing::Values(
        PhaseLockCase{"Clean50Hz", 1000, 10000, 50, 0, 0, 0, 0, 2},
        PhaseLockCase{"Clean60Hz", 1000, 8333, 50, 0, 0, 0, 0, 2},
        // micros() wraps around, and one crossing is exactly at 0
        PhaseLockCase{"CrossingAtZeroMicros", 0U - 30000U, 10000, 50, 0, 0, 0, 0, 2},
        // A detector whose edges alternate early and late (unequal half cycles): the prediction stays in the middle
        PhaseLockCase{"AlternatingJitter", 1000, 10000, 200, 250, 0, 0, 0, 60, false},
        // Locked on edges that already alternate: the lock is on one side, the tracking moves it to the middle
        PhaseLockCase{"AlternatingJitterWhileLocking", 1000, 10000, 200, 250, 0, 0, 0, 60, true},
        PhaseLockCase{"AlternatingJitterWhileLockingAt60Hz", 1000, 8333, 200, 250, 0, 0, 0, 80, true},
        // Noise half way through every half cycle (the triac firing) is outside the window
        PhaseLockCase{"NoiseOutsideWindow", 1000, 10000, 200, 0, 5000, 0, 0, 2},
        // Noise 1 ms before every crossing is inside the window: the edge closest to the
        // prediction is kept, so the noise does not drag the timing
        PhaseLockCase{"NoiseJustBeforeCrossing", 1000, 10000, 200, 0, -1000, 0, 0, 2},
        PhaseLockCase{"MissedCrossing", 1000, 10000, 50, 0, 0, 20, 1, 2},
        PhaseLockCase{"SevenMissedCrossings", 1000, 10000, 50, 0, 0, 20, 7, 2}),
    [](const ::testing::TestParamInfo<PhaseLockCase> &info) { return info.param.name; });

struct EdgeOffsetCase {
  const char *name;
  uint32_t period;
  int32_t offset;  // every edge from crossing `from` up to `to` comes this far from the crossing
  int from;
  int to;
};

class EdgeOffsetTest : public ::testing::TestWithParam<EdgeOffsetCase> {};

// Edges served late for a while (WiFi on the ESP8266) only confirm the crossings, and the timing moved by early
// detector pulses comes back: the prediction ends on the crossings, and a pulse near the end of the half cycle (4 %, no
// minimum power) never fires in the next one, where the triac would stay on for all of it
TEST_P(EdgeOffsetTest, KeepsLockAndPhase) {
  const auto &c = GetParam();
  AcDimmerDataStore store{};
  store.value = 2621;
  Bench bench(store, 1000);
  uint32_t crossing = 1000;
  for (int i = 0; i < 300; i++, crossing += c.period) {
    bench.edge(crossing + (i >= c.from && i < c.to ? c.offset : 0));
    if (i > LOCK_EDGE)
      ASSERT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED) << "crossing " << i;
  }
  const uint32_t last = crossing - c.period;
  bench.run_until(last + c.period / 2);
  EXPECT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED);
  EXPECT_LE(std::abs(static_cast<int32_t>(store.zc_at - last)), 10);
  EXPECT_LE(std::abs(static_cast<int32_t>(store.period - c.period)), 2);
  ASSERT_FALSE(bench.gate_on.empty());
  for (uint32_t at : bench.gate_on)
    EXPECT_GE((at - 1000) % c.period, c.period / 2) << "pulse at " << at;
}

INSTANTIATE_TEST_SUITE_P(AcDimmer, EdgeOffsetTest,
                         ::testing::Values(EdgeOffsetCase{"Late500", 10000, 500, 50, 250},
                                           EdgeOffsetCase{"Late1000", 10000, 1000, 50, 250},
                                           EdgeOffsetCase{"Late1500", 10000, 1500, 50, 250},
                                           EdgeOffsetCase{"Late1500At60Hz", 8333, 1500, 50, 250},
                                           EdgeOffsetCase{"ThreeEarlyPulses", 10000, -470, 100, 103},
                                           // The clean edges after them come late beyond the follow window
                                           EdgeOffsetCase{"ThreeEarlyPulses550", 10000, -550, 100, 103},
                                           EdgeOffsetCase{"ThreeEarlyPulses550At60Hz", 8333, -550, 100, 103}),
                         [](const ::testing::TestParamInfo<EdgeOffsetCase> &info) { return info.param.name; });

struct StartupErrorCase {
  const char *name;
  uint32_t edges[3];  // the first edges while locking; the real crossings are at 1, 11, 21, ... ms
};

class StartupErrorTest : public ::testing::TestWithParam<StartupErrorCase> {};

// Startup edges off the crossings: the timing locks and converges without dropping the lock
TEST_P(StartupErrorTest, ConvergesWhileLocked) {
  const auto &c = GetParam();
  AcDimmerDataStore store{};
  store.value = 32768;
  Bench bench(store, c.edges[0]);
  for (uint32_t at : c.edges)
    bench.edge(at);
  uint32_t crossing = 31000;
  bool locked = false;
  for (int i = 0; i < 300; i++, crossing += 10000) {
    bench.edge(crossing);
    if (locked)
      ASSERT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED) << "crossing " << i;
    locked = store.phase == ZERO_CROSS_PHASE_LOCKED;
  }
  bench.run_until(crossing - 10000 + 5000);
  EXPECT_LE(std::abs(static_cast<int32_t>(store.period - 10000)), 2);
  EXPECT_LE(std::abs(static_cast<int32_t>(store.zc_at - (crossing - 10000))), 20);  // integer steps leave a few µs
}

INSTANTIATE_TEST_SUITE_P(AcDimmer, StartupErrorTest,
                         ::testing::Values(StartupErrorCase{"Short", {1150, 11000, 20850}},
                                           StartupErrorCase{"Long", {1000, 11000, 21300}},
                                           StartupErrorCase{"LateFirst", {1300, 11000, 21000}},
                                           StartupErrorCase{"LateFirst300", {1600, 11000, 21000}},
                                           StartupErrorCase{"LateLast300", {1000, 11000, 21600}}),
                         [](const ::testing::TestParamInfo<StartupErrorCase> &info) { return info.param.name; });

struct GateNoiseCase {
  const char *name;
  uint32_t period;
  int32_t delay;  // real edges from crossing 60 to 160 are served this late
};

class GateNoiseTest : public ::testing::TestWithParam<GateNoiseCase> {};

// At the lowest brightness with the default 10 % minimum power the gate fires about 1 ms before the predicted crossing,
// and its noise on the detector comes with it. While the real edges are served later than that, the noise is the
// edge closest to the prediction, but following it would move the gate and the noise along: neither may drift.
TEST_P(GateNoiseTest, NoiseDoesNotDragTheTiming) {
  const auto &c = GetParam();
  AcDimmerDataStore store{};
  store.value = 655;
  store.min_power = 100;
  Bench bench(store, 1000, 5);
  uint32_t crossing = 1000;
  for (int i = 0; i < 300; i++, crossing += c.period) {
    bench.edge(crossing + (i >= 60 && i < 160 ? c.delay : 0));
    if (i > LOCK_EDGE) {
      ASSERT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED) << "crossing " << i;
      ASSERT_LE(std::abs(static_cast<int32_t>(store.period - c.period)), 10) << "crossing " << i;
    }
  }
  const uint32_t last = crossing - c.period;
  bench.run_until(last + c.period / 2);
  EXPECT_LE(std::abs(static_cast<int32_t>(store.zc_at - last)), 10);
  EXPECT_LE(std::abs(static_cast<int32_t>(store.period - c.period)), 2);
}

INSTANTIATE_TEST_SUITE_P(AcDimmer, GateNoiseTest,
                         ::testing::Values(GateNoiseCase{"Late1100", 10000, 1100},
                                           GateNoiseCase{"Late1500", 10000, 1500},
                                           GateNoiseCase{"Late1100At60Hz", 8333, 1100}),
                         [](const ::testing::TestParamInfo<GateNoiseCase> &info) { return info.param.name; });

/// Whether a gate pulse at `at` started in the first half of a half cycle of crossings at `start` + n * `period`
static bool in_next_half_cycle(uint32_t at, uint32_t start, uint32_t period) {
  return (at - start) % period < period / 2;
}

struct StartupRampCase {
  const char *name;
  int32_t step;  // the first two edges come 2 * step and step early, the rest on the crossings
};

class StartupRampTest : public ::testing::TestWithParam<StartupRampCase> {};

// The first edges after power up drift onto the crossings: the lock waits for them, so the gate is right from the start
TEST_P(StartupRampTest, GateRightFromTheLock) {
  const auto &c = GetParam();
  AcDimmerDataStore store{};
  store.value = 2621;  // 4 %, no minimum power: the gate fires 400 µs before the crossing
  Bench bench(store, 1000);
  uint32_t crossing = 1000;
  for (int i = 0; i < 100; i++, crossing += 10000)
    bench.edge(crossing - (i == 0 ? 2 * c.step : i == 1 ? c.step : 0));
  bench.run_until(crossing);
  ASSERT_FALSE(bench.gate_on.empty());
  EXPECT_LE(bench.gate_on.front() - 1000, 100000U);  // locked within 100 ms
  for (uint32_t at : bench.gate_on)
    EXPECT_LE(std::abs(static_cast<int32_t>((at - 1000) % 10000) - 9600), 20) << "pulse at " << at;
}

INSTANTIATE_TEST_SUITE_P(AcDimmer, StartupRampTest,
                         ::testing::Values(StartupRampCase{"Early150", 150}, StartupRampCase{"Late150", -150},
                                           StartupRampCase{"Early300", 300}, StartupRampCase{"Late300", -300},
                                           StartupRampCase{"Early600", 600}, StartupRampCase{"Late600", -600}),
                         [](const ::testing::TestParamInfo<StartupRampCase> &info) { return info.param.name; });

// A half cycle 50 µs off after the lock comes back while the lock is kept
TEST(AcDimmerTimerTest, WrongHalfCycleConverges) {
  for (int32_t off : {50, -50}) {
    AcDimmerDataStore store{};
    store.value = 2621;
    Bench bench(store, 1000);
    uint32_t crossing = 1000;
    for (int i = 0; i < 300; i++, crossing += 10000) {
      bench.edge(crossing);
      if (i == 20)
        store.period += off;
      if (i > LOCK_EDGE)
        ASSERT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED) << off << " crossing " << i;
    }
    bench.run_until(crossing - 5000);
    EXPECT_LE(std::abs(static_cast<int32_t>(store.period) - 10000), 3) << off;
    EXPECT_LE(std::abs(static_cast<int32_t>(store.zc_at - (crossing - 10000))), 20) << off;
    for (uint32_t at : bench.gate_on)
      EXPECT_FALSE(in_next_half_cycle(at, 1000, 10000)) << off << " pulse at " << at;
  }
}

// ESP32-C3: the timer runs every 50 µs and the detector alternates 250 µs early and late. The gate fires in the
// middle, not on one side of the jitter.
TEST(AcDimmerTimerTest, AlternatingJitterGateOnTickTimer) {
  AcDimmerDataStore store{};
  store.value = 32768;  // the gate is due 4999 µs after the crossing
  Bench bench(store, 1000, 0, 50);
  uint32_t crossing = 1000;
  for (int i = 0; i < 300; i++, crossing += 10000)
    bench.edge(crossing + (i % 2 ? 250 : -250));
  bench.run_until(crossing);
  int32_t lo = INT32_MAX;
  int32_t hi = INT32_MIN;
  for (uint32_t at : bench.gate_on) {
    if (at < 1000000)
      continue;
    const int32_t err = static_cast<int32_t>((at - 1000) % 10000) - 4999;
    lo = std::min(lo, err);
    hi = std::max(hi, err);
  }
  EXPECT_GE(lo, -30);
  EXPECT_LE(hi, 80);
}

/// Locks a dimmer on crossings at 1 to 41 ms; the caller runs the timer at 41 ms to start that half cycle
static AcDimmerDataStore locked_store(DimMethod method, uint16_t value) {
  AcDimmerDataStore store{};
  store.method = method;
  store.value = value;
  for (uint32_t at : {1000U, 11000U, 21000U, 31000U, 41000U})
    store.zero_cross_edge(at);
  return store;
}

// A late or early edge while locking puts off the lock until three full cycles agree, so it does not move the timing
TEST(AcDimmerTimerTest, OffLockingEdgeDoesNotShiftThePrediction) {
  for (uint32_t off : {41600U, 40500U}) {
    AcDimmerDataStore store{};
    for (uint32_t at : {1000U, 11000U, 21000U, 31000U, off, 51000U, 61000U, 71000U, 81000U}) {
      store.zero_cross_edge(at);
      EXPECT_NE(store.phase, ZERO_CROSS_PHASE_LOCKED) << off << " " << at;
    }
    store.zero_cross_edge(91000);
    ASSERT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED) << off;
    EXPECT_EQ(store.period, 10000) << off;
    EXPECT_EQ(store.zc_at, 81000U) << off;
  }
}

TEST(AcDimmerTimerTest, NothingBeforeTheLock) {
  AcDimmerDataStore store{};
  store.value = 32768;
  for (uint32_t at : {1000U, 11000U, 21000U, 31000U}) {
    store.zero_cross_edge(at);
    EXPECT_NE(store.phase, ZERO_CROSS_PHASE_LOCKED);
    EXPECT_EQ(store.timer_intr(at + 100), 0U);
  }
}

// A gap longer than any half cycle while locking starts the measurement over
TEST(AcDimmerTimerTest, LongGapStartsTheLockOver) {
  AcDimmerDataStore store{};
  for (uint32_t at : {1000U, 11000U, 41000U, 51000U, 61000U, 71000U}) {
    store.zero_cross_edge(at);
    EXPECT_NE(store.phase, ZERO_CROSS_PHASE_LOCKED);
  }
  store.zero_cross_edge(81000);
  EXPECT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED);
  EXPECT_EQ(store.period, 10000);
}

// A crossing has no edge for eight half cycles in a row: the lock is dropped and the timer goes idle
TEST(AcDimmerTimerTest, EightMissesDropTheLock) {
  auto store = locked_store(DIM_METHOD_LEADING_PULSE, 32768);
  store.timer_intr(41000);
  store.timer_intr(51000);  // the crossing at 41 ms had its edge
  for (uint32_t at = 61000; at < 131000; at += 10000) {
    EXPECT_NE(store.timer_intr(at), 0U);
    EXPECT_EQ(store.phase, ZERO_CROSS_PHASE_LOCKED);
    EXPECT_EQ(store.zc_at, at);
  }
  EXPECT_EQ(store.timer_intr(131000), 0U);
  EXPECT_EQ(store.phase, ZERO_CROSS_PHASE_NONE);
  EXPECT_EQ(store.timer_intr(131100), 0U);
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
  EXPECT_EQ(store.timer_intr(41000), c.first_event_in);
  EXPECT_EQ(store.zc_at, 41000U);
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

// A timer delayed by masked sections must not start a pulse that lasts past the crossing: the triac would stay
// on into the next half cycle
TEST_P(LateEnableTest, SkipsEnableNearTheEnd) {
  const auto &c = GetParam();
  auto store = locked_store(DIM_METHOD_LEADING_PULSE, 2621);
  ASSERT_EQ(store.timer_intr(41000), 9600U);
  EXPECT_EQ(store.timer_intr(41000 + c.timer_at), c.next_event_in);
  EXPECT_EQ(store.enable_time_us, 0U);
  EXPECT_EQ(store.disable_time_us, c.disable_time_us);
}

INSTANTIATE_TEST_SUITE_P(AcDimmer, LateEnableTest,
                         ::testing::Values(LateEnableCase{"OnTime", 9600, 9650, 50},
                                           LateEnableCase{"LatePulseEndsBeforeCrossing", 9800, 9850, 50},
                                           // not switched on; the pending switch-off runs at once
                                           LateEnableCase{"LatePulseWouldPassCrossing", 9960, 0, 40}),
                         [](const ::testing::TestParamInfo<LateEnableCase> &info) { return info.param.name; });

}  // namespace esphome::ac_dimmer::testing
