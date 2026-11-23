#include <gtest/gtest.h>

#include <cstring>

#include "esphome/components/vl53l4cx/vl53l4cx.h"

namespace esphome::vl53l4cx::testing {

struct Range {
  int16_t mm;
  uint8_t status;
  uint32_t signal;  // FixPoint1616 Mcps
};

static VL53L4CX_MultiRangingData_t frame(std::initializer_list<Range> ranges) {
  VL53L4CX_MultiRangingData_t data;
  memset(&data, 0, sizeof(data));
  for (const Range &range : ranges) {
    VL53L4CX_TargetRangeData_t &out = data.RangeData[data.NumberOfObjectsFound++];
    out.RangeMilliMeter = range.mm;
    out.RangeStatus = range.status;
    out.SignalRateRtnMegaCps = range.signal;
  }
  return data;
}

static constexpr uint8_t VALID = VL53L4CX_RANGESTATUS_RANGE_VALID;
static constexpr uint8_t SIGMA_FAIL = VL53L4CX_RANGESTATUS_SIGMA_FAIL;

// A box under part of the field of view and the ceiling past it, as the bench sees them
static const VL53L4CX_MultiRangingData_t TOWER_AND_CEILING = frame({{215, VALID, 5 << 16}, {2391, VALID, 1 << 15}});

TEST(VL53L4CXSelect, NearestFarthestAndStrongestOfTwoTargets) {
  EXPECT_EQ(select_distance_mm(TOWER_AND_CEILING, TARGET_NEAREST, 0, 0), 215);
  EXPECT_EQ(select_distance_mm(TOWER_AND_CEILING, TARGET_FARTHEST, 0, 0), 2391);
  EXPECT_EQ(select_distance_mm(TOWER_AND_CEILING, TARGET_STRONGEST, 0, 0), 215);
}

// ratgdo's rule: the farthest valid target past 100 mm, so a partial obstruction does not hide the floor
TEST(VL53L4CXSelect, MinDistanceSkipsTargetsOnTheCoverGlass) {
  const auto data = frame({{60, VALID, 11 << 16}, {1800, VALID, 1 << 16}});
  EXPECT_EQ(select_distance_mm(data, TARGET_NEAREST, 100, 0), 1800);
  EXPECT_EQ(select_distance_mm(data, TARGET_STRONGEST, 100, 0), 1800);
  // Only the near target: nothing to report
  EXPECT_EQ(select_distance_mm(frame({{66, VALID, 11 << 16}}), TARGET_FARTHEST, 100, 0), -1);
}

TEST(VL53L4CXSelect, InvalidTargetsAreSkipped) {
  const auto data = frame({{400, SIGMA_FAIL, 9 << 16}, {1200, VALID, 1 << 16}, {3000, SIGMA_FAIL, 1 << 16}});
  EXPECT_EQ(select_distance_mm(data, TARGET_NEAREST, 0, 0), 1200);
  EXPECT_EQ(select_distance_mm(data, TARGET_FARTHEST, 0, 0), 1200);
  EXPECT_EQ(select_distance_mm(data, TARGET_STRONGEST, 0, 0), 1200);
  EXPECT_EQ(select_distance_mm(frame({{900, SIGMA_FAIL, 1 << 16}}), TARGET_NEAREST, 0, 0), -1);
  EXPECT_EQ(select_distance_mm(frame({}), TARGET_NEAREST, 0, 0), -1);
}

// ST reports clipped, first (no wrap check yet) and merged-pulse ranges as valid too
TEST(VL53L4CXSelect, AllValidStatusesCount) {
  for (uint8_t status :
       {VL53L4CX_RANGESTATUS_RANGE_VALID_MIN_RANGE_CLIPPED, VL53L4CX_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK_FAIL,
        VL53L4CX_RANGESTATUS_RANGE_VALID_MERGED_PULSE}) {
    EXPECT_EQ(select_distance_mm(frame({{1500, status, 1 << 16}}), TARGET_NEAREST, 0, 0), 1500) << int(status);
  }
}

// The offset applies before the minimum distance check
TEST(VL53L4CXSelect, OffsetAppliesBeforeMinDistance) {
  const auto data = frame({{110, VALID, 1 << 16}});
  EXPECT_EQ(select_distance_mm(data, TARGET_NEAREST, 100, 15), 125);
  EXPECT_EQ(select_distance_mm(data, TARGET_NEAREST, 100, -15), -1);
}

}  // namespace esphome::vl53l4cx::testing
