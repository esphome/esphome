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
        // 50 Hz; the first edge only starts the timing
        ZeroCrossCase{"Regular50Hz",
                      {{1000, false, 1000, 0}, {11000, true, 11000, 10000}, {21000, true, 21000, 10000}}},
        ZeroCrossCase{"Regular60Hz", {{1000, false, 1000, 0}, {9333, true, 9333, 8333}, {17666, true, 17666, 8333}}},
        // The second edge of the same zero-cross pulse, and noise in the middle of the half cycle
        ZeroCrossCase{"NoiseIgnored",
                      {{1000, false, 1000, 0},
                       {11000, true, 11000, 10000},
                       {11500, false, 11000, 10000},
                       {14000, false, 11000, 10000},
                       {18000, false, 11000, 10000},
                       {21000, true, 21000, 10000},
                       {21400, false, 21000, 10000},
                       {31000, true, 31000, 10000}}},
        // The zero-cross pulse starts half a millisecond early: the expected crossing time is used, with or
        // without an edge at the real crossing
        ZeroCrossCase{"EarlyPulseUsesExpectedTime",
                      {{1000, false, 1000, 0},
                       {11000, true, 11000, 10000},
                       {20460, true, 21000, 10000},
                       {20990, false, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {40500, true, 41000, 10000},
                       {51000, true, 51000, 10000}}},
        // The interrupt was served late: the crossing was at the expected time
        ZeroCrossCase{"LateEdgeUsesExpectedTime",
                      {{1000, false, 1000, 0},
                       {11000, true, 11000, 10000},
                       {21450, true, 21000, 10000},
                       {31000, true, 31000, 10000},
                       {41000, true, 41000, 10000}}},
        // An edge 0.3 ms late moves the timing only by half of that, and the next edges pull it back
        ZeroCrossCase{"JitterFollowedSlowly",
                      {{1000, false, 1000, 0},
                       {11000, true, 11000, 10000},
                       {21300, true, 21150, 10018},
                       {31000, true, 31084, 10008},
                       {41000, true, 41046, 10003},
                       {51000, true, 51024, 10000}}},
        // One crossing has no edge at all
        ZeroCrossCase{"MissedCrossing",
                      {{1000, false, 1000, 0},
                       {11000, true, 11000, 10000},
                       {31000, true, 31000, 10000},
                       {41000, true, 41000, 10000}}},
        // The half cycle is first measured from a noise edge: after eight edges that do not fit it is measured again
        ZeroCrossCase{"WrongFirstMeasurementRecovers",
                      {{1000, false, 1000, 0},
                       {8000, true, 8000, 7000},
                       {11000, false, 8000, 7000},
                       {21000, true, 22000, 7000},
                       {31000, true, 29000, 7000},
                       {41000, false, 29000, 7000},
                       {51000, true, 50000, 7000},
                       {61000, false, 50000, 7000},
                       {71000, true, 71000, 7000},
                       {81000, true, 78000, 7000},
                       {91000, false, 91000, 0},
                       {101000, true, 101000, 10000},
                       {111000, true, 111000, 10000}}},
        // The half cycle is first measured over a missed crossing
        ZeroCrossCase{"DoubleHalfCycleRecovers",
                      {{1000, false, 1000, 0},         {21000, true, 21000, 20000},    {31000, false, 21000, 20000},
                       {41000, true, 41000, 20000},    {51000, false, 41000, 20000},   {61000, true, 61000, 20000},
                       {71000, false, 61000, 20000},   {81000, true, 81000, 20000},    {91000, false, 81000, 20000},
                       {101000, true, 101000, 20000},  {111000, false, 101000, 20000}, {121000, true, 121000, 20000},
                       {131000, false, 121000, 20000}, {141000, true, 141000, 20000},  {151000, false, 151000, 0},
                       {161000, true, 161000, 10000},  {171000, true, 171000, 10000},  {181000, true, 181000, 10000},
                       {191000, true, 191000, 10000},  {201000, true, 201000, 10000}}},
        // No edges for a long time
        ZeroCrossCase{"LongGapMeasuresAgain",
                      {{1000, false, 1000, 0},
                       {11000, true, 11000, 10000},
                       {21000, true, 21000, 10000},
                       {124400, false, 124400, 0},
                       {134400, true, 134400, 10000},
                       {144400, true, 144400, 10000}}},
        // micros() wraps around
        ZeroCrossCase{"Wraparound",
                      {{0xFFFFE000, false, 0xFFFFE000, 0},
                       {0x00000710, true, 0x00000710, 10000},
                       {0x00000910, false, 0x00000710, 10000},
                       {0x00002E20, true, 0x00002E20, 10000}}}),
    [](const ::testing::TestParamInfo<ZeroCrossCase> &info) { return info.param.name; });

TEST(AcDimmerTimerTest, WaitsForExpectedCrossing) {
  AcDimmerDataStore store{};
  store.value = 30000;
  store.cycle_time_us = 10000;
  store.enable_time_us = 5000;
  store.crossed_zero_at = 21000;
  EXPECT_EQ(store.timer_intr(20460), 540u);
  EXPECT_EQ(store.timer_intr(22000), 4000u);
}

}  // namespace esphome::ac_dimmer::testing
