#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/ac_dimmer/ac_dimmer.h"

namespace esphome::ac_dimmer::testing {

struct Edge {
  uint32_t at_us;
  bool taken;
  uint32_t crossed_zero_at;
  uint32_t cycle_time_us;
};

struct ZeroCrossCase {
  const char *name;
  std::vector<Edge> edges;
};

class ZeroCrossTest : public ::testing::TestWithParam<ZeroCrossCase> {};

TEST_P(ZeroCrossTest, TracksCrossings) {
  AcDimmerDataStore store{};
  for (const auto &edge : GetParam().edges) {
    SCOPED_TRACE(edge.at_us);
    EXPECT_EQ(store.take_zero_cross_edge(edge.at_us), edge.taken);
    EXPECT_EQ(store.crossed_zero_at, edge.crossed_zero_at);
    EXPECT_EQ(store.cycle_time_us, edge.cycle_time_us);
  }
}

INSTANTIATE_TEST_SUITE_P(
    AcDimmer, ZeroCrossTest,
    ::testing::Values(
        // 50 Hz: the first edge starts the measurement, the second gives the half cycle, two more confirm it
        ZeroCrossCase{"Regular50Hz",
                      {{1000, false, 1000, 0},
                       {11000, false, 11000, 10000},
                       {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {41000, true, 41000, 10000},
                       {51000, true, 51000, 10000}}},
        // 60 Hz
        ZeroCrossCase{"Regular60Hz",
                      {{1000, false, 1000, 0},
                       {9333, false, 9333, 8333},
                       {17666, false, 17666, 8333},
                       {25999, true, 25999, 8333},
                       {34332, true, 34332, 8333},
                       {42665, true, 42665, 8333}}},
        // The second edge of the same zero-cross pulse, and noise in the middle of the half cycle
        ZeroCrossCase{"NoiseIgnored",
                      {{1000, false, 1000, 0},
                       {11000, false, 11000, 10000},
                       {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {31500, false, 31000, 10000},
                       {34000, false, 31000, 10000},
                       {38000, false, 31000, 10000},
                       {41000, true, 41000, 10000},
                       {41400, false, 41000, 10000},
                       {51000, true, 51000, 10000}}},
        // The zero-cross pulse starts half a millisecond early: the expected crossing time is used, with or
        // without an edge at the real crossing
        ZeroCrossCase{"EarlyPulseUsesExpectedTime",
                      {{1000, false, 1000, 0},
                       {11000, false, 11000, 10000},
                       {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {40460, true, 41000, 10000},
                       {40990, false, 40995, 10000},
                       {51000, true, 50998, 10000},
                       {60500, true, 60998, 10000},
                       {71000, true, 70999, 10000}}},
        // The interrupt was served late: the crossing was at the expected time
        ZeroCrossCase{"LateEdgeUsesExpectedTime",
                      {{1000, false, 1000, 0},
                       {11000, false, 11000, 10000},
                       {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {41450, true, 41000, 10000},
                       {51000, true, 51000, 10000},
                       {61000, true, 61000, 10000}}},
        // An edge 0.3 ms late moves the timing only by half of that, and the next edges pull it back
        ZeroCrossCase{"JitterFollowedSlowly",
                      {{1000, false, 1000, 0},
                       {11000, false, 11000, 10000},
                       {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {41300, true, 41150, 10018},
                       {51000, true, 51084, 10008},
                       {61000, true, 61046, 10003},
                       {71000, true, 71024, 10000}}},
        // One crossing has no edge at all
        ZeroCrossCase{"MissedCrossing",
                      {{1000, false, 1000, 0},
                       {11000, false, 11000, 10000},
                       {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {51000, true, 51000, 10000},
                       {61000, true, 61000, 10000}}},
        // The half cycle is first measured from a noise edge: the next edge does not confirm it
        ZeroCrossCase{"WrongFirstMeasurementRecovers",
                      {{1000, false, 1000, 0},
                       {8000, false, 8000, 7000},
                       {11000, false, 8000, 7000},
                       {21000, false, 21000, 0},
                       {31000, false, 31000, 10000},
                       {41000, false, 41000, 10000},
                       {51000, true, 51000, 10000}}},
        // A first measurement of two thirds of the half cycle agrees with every third crossing, two half cycles on:
        // that does not confirm it
        ZeroCrossCase{"TwoThirdsFirstMeasurementRecovers",
                      {{1000, false, 1000, 0},
                       {7667, false, 7667, 6667},
                       {11000, false, 7667, 6667},
                       {21000, false, 21000, 0},
                       {31000, false, 31000, 10000},
                       {41000, false, 41000, 10000},
                       {51000, true, 51000, 10000},
                       {61000, true, 61000, 10000}}},
        // The half cycle is first measured over a missed crossing
        ZeroCrossCase{"DoubleHalfCycleRecovers",
                      {{1000, false, 1000, 0},
                       {21000, false, 21000, 20000},
                       {31000, false, 31000, 0},
                       {41000, false, 41000, 10000},
                       {51000, false, 51000, 10000},
                       {61000, true, 61000, 10000},
                       {71000, true, 71000, 10000}}},
        // No edges for a long time
        ZeroCrossCase{"LongGapMeasuresAgain",
                      {{1000, false, 1000, 0},
                       {11000, false, 11000, 10000},
                       {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {134400, false, 134400, 0},
                       {144400, false, 144400, 10000},
                       {154400, false, 154400, 10000},
                       {164400, true, 164400, 10000}}},
        // A long gap that happens to be a whole number of half cycles is a gap all the same
        ZeroCrossCase{"ExactMultipleGapMeasuresAgain",
                      {{1000, false, 1000, 0},
                       {11000, false, 11000, 10000},
                       {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {131000, false, 131000, 0},
                       {141000, false, 141000, 10000},
                       {151000, false, 151000, 10000},
                       {161000, true, 161000, 10000}}},
        // Noise 7 ms after every crossing (the triac firing, coupled into the detector): the crossings one half cycle
        // apart keep the timing, however many spikes there are
        ZeroCrossCase{"RecurringSpikesDoNotTakeOver",
                      {{1000, false, 1000, 0},         {11000, false, 11000, 10000},   {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},    {41000, true, 41000, 10000},    {48000, false, 41000, 10000},
                       {51000, true, 51000, 10000},    {58000, false, 51000, 10000},   {61000, true, 61000, 10000},
                       {68000, false, 61000, 10000},   {71000, true, 71000, 10000},    {78000, false, 71000, 10000},
                       {81000, true, 81000, 10000},    {88000, false, 81000, 10000},   {91000, true, 91000, 10000},
                       {98000, false, 91000, 10000},   {101000, true, 101000, 10000},  {108000, false, 101000, 10000},
                       {111000, true, 111000, 10000},  {118000, false, 111000, 10000}, {121000, true, 121000, 10000},
                       {128000, false, 121000, 10000}, {131000, true, 131000, 10000},  {138000, false, 131000, 10000},
                       {141000, true, 141000, 10000},  {148000, false, 141000, 10000}, {151000, true, 151000, 10000},
                       {158000, false, 151000, 10000}, {161000, true, 161000, 10000},  {168000, false, 161000, 10000},
                       {171000, true, 171000, 10000},  {178000, false, 171000, 10000}, {181000, true, 181000, 10000},
                       {188000, false, 181000, 10000}, {191000, true, 191000, 10000},  {198000, false, 191000, 10000},
                       {201000, true, 201000, 10000},  {208000, false, 201000, 10000}, {211000, true, 211000, 10000},
                       {218000, false, 211000, 10000}, {221000, true, 221000, 10000},  {228000, false, 221000, 10000},
                       {231000, true, 231000, 10000},  {238000, false, 231000, 10000}, {241000, true, 241000, 10000},
                       {248000, false, 241000, 10000}, {251000, true, 251000, 10000},  {258000, false, 251000, 10000},
                       {261000, true, 261000, 10000},  {268000, false, 261000, 10000}, {271000, true, 271000, 10000},
                       {278000, false, 271000, 10000}, {281000, true, 281000, 10000},  {288000, false, 281000, 10000},
                       {291000, true, 291000, 10000},  {298000, false, 291000, 10000}}},
        // The crossings move by 4 ms at once (a generator switching in): eight edges in a row that do not fit, then a
        // new measurement
        ZeroCrossCase{"LostTimingMeasuredAgain",
                      {{1000, false, 1000, 0},
                       {11000, false, 11000, 10000},
                       {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {45000, true, 41000, 10000},
                       {55000, true, 51000, 10000},
                       {65000, true, 61000, 10000},
                       {75000, true, 71000, 10000},
                       {85000, true, 81000, 10000},
                       {95000, true, 91000, 10000},
                       {105000, true, 101000, 10000},
                       {115000, false, 115000, 0},
                       {125000, false, 125000, 10000},
                       {135000, false, 135000, 10000},
                       {145000, true, 145000, 10000},
                       {155000, true, 155000, 10000},
                       {165000, true, 165000, 10000}}},
        // An edge 2 ms before every crossing (the triac firing at low brightness): it is taken as an early pulse at the
        // expected time, and the real edge there confirms the timing each time
        ZeroCrossCase{"RecurringEarlyEdgesDoNotTakeOver",
                      {{1000, false, 1000, 0},         {11000, false, 11000, 10000},   {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},    {39000, true, 41000, 10000},    {41000, false, 41000, 10000},
                       {49000, true, 51000, 10000},    {51000, false, 51000, 10000},   {59000, true, 61000, 10000},
                       {61000, false, 61000, 10000},   {69000, true, 71000, 10000},    {71000, false, 71000, 10000},
                       {79000, true, 81000, 10000},    {81000, false, 81000, 10000},   {89000, true, 91000, 10000},
                       {91000, false, 91000, 10000},   {99000, true, 101000, 10000},   {101000, false, 101000, 10000},
                       {109000, true, 111000, 10000},  {111000, false, 111000, 10000}, {119000, true, 121000, 10000},
                       {121000, false, 121000, 10000}, {129000, true, 131000, 10000},  {131000, false, 131000, 10000},
                       {139000, true, 141000, 10000},  {141000, false, 141000, 10000}, {149000, true, 151000, 10000},
                       {151000, false, 151000, 10000}, {159000, true, 161000, 10000},  {161000, false, 161000, 10000},
                       {169000, true, 171000, 10000},  {171000, false, 171000, 10000}, {179000, true, 181000, 10000},
                       {181000, false, 181000, 10000}, {189000, true, 191000, 10000},  {191000, false, 191000, 10000},
                       {199000, true, 201000, 10000},  {201000, false, 201000, 10000}, {209000, true, 211000, 10000},
                       {211000, false, 211000, 10000}, {219000, true, 221000, 10000},  {221000, false, 221000, 10000},
                       {229000, true, 231000, 10000},  {231000, false, 231000, 10000}, {239000, true, 241000, 10000},
                       {241000, false, 241000, 10000}, {249000, true, 251000, 10000},  {251000, false, 251000, 10000},
                       {259000, true, 261000, 10000},  {261000, false, 261000, 10000}, {269000, true, 271000, 10000},
                       {271000, false, 271000, 10000}}},
        // The same at 60 Hz, 1.3 ms before every crossing
        ZeroCrossCase{"RecurringEarlyEdges60Hz",
                      {{1000, false, 1000, 0},        {9333, false, 9333, 8333},     {17666, false, 17666, 8333},
                       {25999, true, 25999, 8333},    {32999, true, 34332, 8333},    {34332, false, 34332, 8333},
                       {41332, true, 42665, 8333},    {42665, false, 42665, 8333},   {49665, true, 50998, 8333},
                       {50998, false, 50998, 8333},   {57998, true, 59331, 8333},    {59331, false, 59331, 8333},
                       {66331, true, 67664, 8333},    {67664, false, 67664, 8333},   {74664, true, 75997, 8333},
                       {75997, false, 75997, 8333},   {82997, true, 84330, 8333},    {84330, false, 84330, 8333},
                       {91330, true, 92663, 8333},    {92663, false, 92663, 8333},   {99663, true, 100996, 8333},
                       {100996, false, 100996, 8333}, {107996, true, 109329, 8333},  {109329, false, 109329, 8333},
                       {116329, true, 117662, 8333},  {117662, false, 117662, 8333}, {124662, true, 125995, 8333},
                       {125995, false, 125995, 8333}, {132995, true, 134328, 8333},  {134328, false, 134328, 8333},
                       {141328, true, 142661, 8333},  {142661, false, 142661, 8333}, {149661, true, 150994, 8333},
                       {150994, false, 150994, 8333}, {157994, true, 159327, 8333},  {159327, false, 159327, 8333},
                       {166327, true, 167660, 8333},  {167660, false, 167660, 8333}, {174660, true, 175993, 8333},
                       {175993, false, 175993, 8333}, {182993, true, 184326, 8333},  {184326, false, 184326, 8333},
                       {191326, true, 192659, 8333},  {192659, false, 192659, 8333}, {199659, true, 200992, 8333},
                       {200992, false, 200992, 8333}, {207992, true, 209325, 8333},  {209325, false, 209325, 8333},
                       {216325, true, 217658, 8333},  {217658, false, 217658, 8333}, {224658, true, 225991, 8333},
                       {225991, false, 225991, 8333}}},
        // No edges for 40 minutes: the time of the last crossing looks as if it were ahead; it is stale, measure again
        ZeroCrossCase{"StaleTimeAfterLongOutage",
                      {{1000, false, 1000, 0},
                       {11000, false, 11000, 10000},
                       {21000, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {0x8F0D9118, false, 0x8F0D9118, 0},
                       {0x8F0DB828, false, 0x8F0DB828, 10000},
                       {0x8F0DDF38, false, 0x8F0DDF38, 10000},
                       {0x8F0E0648, true, 0x8F0E0648, 10000}}},
        // micros() wraps around
        ZeroCrossCase{"Wraparound",
                      {{0xFFFFB000, false, 0xFFFFB000, 0},
                       {0xFFFFD710, false, 0xFFFFD710, 10000},
                       {0xFFFFFE20, false, 0xFFFFFE20, 10000},
                       {9520, true, 9520, 10000},
                       {19520, true, 19520, 10000},
                       {29520, true, 29520, 10000}}}),
    [](const ::testing::TestParamInfo<ZeroCrossCase> &info) { return info.param.name; });

// An edge 230 us before every crossing, close enough to pass for the crossing itself (the triac firing at the
// lowest brightness): the real edge after it pulls the timing back each time, so it stays put
TEST(AcDimmerZeroCrossTest, EdgeJustBeforeEveryCrossingDoesNotDragTheTiming) {
  AcDimmerDataStore store{};
  for (uint32_t at : {1000U, 11000U, 21000U, 31000U})
    store.take_zero_cross_edge(at);
  for (uint32_t crossing = 41000; crossing < 41000 + 500 * 10000; crossing += 10000) {
    SCOPED_TRACE(crossing);
    EXPECT_TRUE(store.take_zero_cross_edge(crossing - 230));
    EXPECT_FALSE(store.take_zero_cross_edge(crossing));
    EXPECT_NEAR(static_cast<double>(store.crossed_zero_at), static_cast<double>(crossing), 130);
    EXPECT_NEAR(static_cast<double>(store.cycle_time_us), 10000, 20);
  }
}

// A leading-pulse dimmer whose zero-cross timing is confirmed: crossings at 1, 11, 21 and 31 ms
static AcDimmerDataStore locked_store(uint16_t value) {
  AcDimmerDataStore store{};
  store.value = value;
  store.method = DIM_METHOD_LEADING_PULSE;
  for (uint32_t at : {1000U, 11000U, 21000U, 31000U})
    store.zero_cross_edge(at);
  return store;
}

TEST(AcDimmerScheduleTest, StartsWhenTheTimingIsConfirmed) {
  AcDimmerDataStore store{};
  store.value = 32768;
  store.method = DIM_METHOD_LEADING_PULSE;
  for (uint32_t at : {1000U, 11000U, 21000U}) {
    store.zero_cross_edge(at);
    EXPECT_FALSE(store.timing_locked());
    EXPECT_EQ(store.enable_time_us, 0U);
    EXPECT_EQ(store.timer_intr(at + 100), 0U);
  }
  store.zero_cross_edge(31000);
  EXPECT_TRUE(store.timing_locked());
  EXPECT_EQ(store.schedule_zero_at, 31000U);
  EXPECT_EQ(store.enable_time_us, 4999U);
  EXPECT_EQ(store.disable_time_us, 5049U);
  EXPECT_EQ(store.timer_intr(31000), 4999U);
}

// An early zero-cross pulse must not replace the gate events of the half cycle that is still running
TEST(AcDimmerScheduleTest, EarlyEdgeKeepsRunningSchedule) {
  auto store = locked_store(2621);  // gate pulse 9.6 ms after the crossing
  ASSERT_EQ(store.enable_time_us, 9600U);
  ASSERT_EQ(store.disable_time_us, 9650U);

  store.zero_cross_edge(40520);  // the pulse of the crossing at 41 ms starts 480 us early
  EXPECT_EQ(store.crossed_zero_at, 41000U);
  EXPECT_EQ(store.schedule_zero_at, 31000U);
  EXPECT_EQ(store.enable_time_us, 9600U);
  EXPECT_EQ(store.timer_intr(40520), 80U);  // the gate pulse of the running half cycle is next

  EXPECT_EQ(store.timer_intr(40600), 50U);  // gate on
  EXPECT_EQ(store.enable_time_us, 0U);
  EXPECT_EQ(store.disable_time_us, 9650U);
  EXPECT_EQ(store.timer_intr(40650), 350U);  // gate off, then wait for the expected crossing
  EXPECT_EQ(store.disable_time_us, 0U);

  EXPECT_EQ(store.timer_intr(41000), 9600U);  // the new half cycle starts at the expected crossing
  EXPECT_EQ(store.schedule_zero_at, 41000U);
  EXPECT_EQ(store.enable_time_us, 9600U);
  EXPECT_EQ(store.disable_time_us, 9650U);
}

// An edge served late starts the half cycle at once, counted from the expected crossing
TEST(AcDimmerScheduleTest, LateEdgeStartsAtExpectedCrossing) {
  auto store = locked_store(32768);
  store.zero_cross_edge(41450);
  EXPECT_EQ(store.schedule_zero_at, 41000U);
  EXPECT_EQ(store.timer_intr(41450), 4549U);
}

// When the timing is lost, pending gate events are dropped: counted from the edge the new measurement starts at
// they could hold the gate on across a crossing
TEST(AcDimmerScheduleTest, LostTimingCancelsSchedule) {
  auto store = locked_store(32768);
  EXPECT_EQ(store.timer_intr(36000), 50U);  // gate on, off in 50 us
  ASSERT_NE(store.disable_time_us, 0U);

  store.zero_cross_edge(131000);  // a gap: measured again
  EXPECT_FALSE(store.timing_locked());
  EXPECT_EQ(store.enable_time_us, 0U);
  EXPECT_EQ(store.disable_time_us, 0U);
  EXPECT_EQ(store.timer_intr(131100), 0U);
}

// The real edge of an early pulse moves the crossing it confirms, and the half cycle already started there
TEST(AcDimmerScheduleTest, ConfirmingEdgeMovesStartedHalfCycle) {
  auto store = locked_store(32768);
  store.zero_cross_edge(40520);
  EXPECT_EQ(store.timer_intr(41000), 4999U);  // started at the expected crossing
  ASSERT_EQ(store.schedule_zero_at, 41000U);

  store.zero_cross_edge(41010);  // the real edge, 10 us after the expected time
  EXPECT_EQ(store.crossed_zero_at, 41005U);
  EXPECT_EQ(store.schedule_zero_at, 41005U);
  EXPECT_EQ(store.enable_time_us, 4999U);  // the events themselves are not set up again
}

// On the ESP32 the zero-cross interrupt can start a half cycle after the timer interrupt has read the time
TEST(AcDimmerScheduleTest, NothingDueBeforeTheStartOfTheHalfCycle) {
  auto store = locked_store(32768);
  EXPECT_EQ(store.timer_intr(30990), 0U);
  EXPECT_EQ(store.enable_time_us, 4999U);
  EXPECT_EQ(store.disable_time_us, 5049U);
}

// Fully on and fully off have no gate events, but an early pulse still waits for the expected crossing
TEST(AcDimmerScheduleTest, EarlyEdgeWithoutGateEvents) {
  for (uint16_t value : {uint16_t{0}, uint16_t{65535}}) {
    auto store = locked_store(value);
    store.zero_cross_edge(40520);
    EXPECT_EQ(store.schedule_zero_at, 31000U);
    EXPECT_EQ(store.timer_intr(40520), 480U);
    EXPECT_EQ(store.timer_intr(41000), 0U);
    EXPECT_EQ(store.schedule_zero_at, 41000U);
  }
}

}  // namespace esphome::ac_dimmer::testing
