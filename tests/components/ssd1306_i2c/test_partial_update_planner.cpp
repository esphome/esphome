#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <utility>
#include "esphome/components/ssd1306_i2c/ssd1306_partial_update.h"

namespace esphome::ssd1306_i2c {
namespace {
using Frame = std::array<uint8_t, 1024>;

uint32_t independent_cost(const Region &region) {
  const uint32_t bytes = (region.max_x - region.min_x + 1) * (region.max_page - region.min_page + 1);
  return 72 + bytes * 9 + ((bytes + 15) / 16) * 18;
}

void verify_plan(const Frame &current, const Frame &previous, uint16_t width, uint8_t pages,
                 const PartialUpdatePlanner &planner) {
  std::array<bool, 1024> covered{};
  ASSERT_LE(planner.get_region_count(), PartialUpdatePlanner::MAX_PLANNED_REGIONS);
  uint32_t cost = 0;
  for (size_t i = 0; i < planner.get_region_count(); i++) {
    const auto &region = planner.get_region(i);
    ASSERT_LE(region.min_x, region.max_x);
    ASSERT_LE(region.min_page, region.max_page);
    ASSERT_TRUE(region.max_x < width);
    ASSERT_TRUE(region.max_page < pages);
    cost += independent_cost(region);
    for (unsigned page = region.min_page; page <= region.max_page; page++) {
      for (unsigned x = region.min_x; x <= region.max_x; x++) {
        covered[page * width + x] = true;
      }
    }
  }
  EXPECT_EQ(cost, planner.get_wire_clocks());
  bool changed = false;
  Region bounds{127, 0, 7, 0};
  uint32_t page_cost = 0;
  for (uint8_t page = 0; page < pages; page++) {
    Region page_bounds{127, 0, page, page};
    bool page_changed = false;
    for (uint16_t x = 0; x < width; x++) {
      const size_t index = page * width + x;
      if (current[index] != previous[index]) {
        ASSERT_TRUE(covered[index]);
        changed = true;
        page_changed = true;
        bounds.min_x = std::min(bounds.min_x, static_cast<uint8_t>(x));
        bounds.max_x = std::max(bounds.max_x, static_cast<uint8_t>(x));
        bounds.min_page = std::min(bounds.min_page, page);
        bounds.max_page = std::max(bounds.max_page, page);
        page_bounds.min_x = std::min(page_bounds.min_x, static_cast<uint8_t>(x));
        page_bounds.max_x = std::max(page_bounds.max_x, static_cast<uint8_t>(x));
      }
    }
    if (page_changed) {
      page_cost += independent_cost(page_bounds);
    }
  }
  if (!changed) {
    EXPECT_EQ(planner.get_region_count(), 0U);
  } else {
    EXPECT_LE(cost, independent_cost(bounds));
    if (!planner.used_fallback()) {
      EXPECT_LE(cost, page_cost);
    }
  }
}

void expect_region(const Region &actual, const Region &expected) {
  EXPECT_EQ(actual.min_x, expected.min_x);
  EXPECT_EQ(actual.max_x, expected.max_x);
  EXPECT_EQ(actual.min_page, expected.min_page);
  EXPECT_EQ(actual.max_page, expected.max_page);
}

class PagePlanner : public PartialUpdatePlanner {
 public:
  // Initialize unused output slots so a rejected append can be checked for unintended writes.
  PagePlanner() { this->regions_.fill(Region{255, 255, 255, 255}); }
  void set_capacity(size_t capacity) { this->capacity_ = capacity; }
  bool append_page(uint8_t page, const uint8_t *run_starts, const uint8_t *run_ends, size_t run_count) {
    return this->add_optimized_page_regions_(page, run_starts, run_ends, run_count);
  }
};

TEST(SSD1306Planner, UnchangedAndReusedWorkspace) {
  Frame current{};
  Frame previous{};
  PartialUpdatePlanner planner;
  current[1023] = 1;
  ASSERT_TRUE(planner.build(current.data(), previous.data(), 128, 8));
  EXPECT_EQ(planner.get_region_count(), 1U);
  previous = current;
  ASSERT_TRUE(planner.build(current.data(), previous.data(), 128, 8));
  EXPECT_EQ(planner.get_region_count(), 0U);
  EXPECT_EQ(planner.get_wire_clocks(), 0U);
  EXPECT_FALSE(planner.used_fallback());
}

TEST(SSD1306Planner, EverySingleByteForEverySupportedGeometry) {
  const std::array<std::pair<uint16_t, uint8_t>, 6> dimensions{
      {{128, 4}, {128, 8}, {96, 2}, {64, 6}, {64, 4}, {72, 5}}};
  Frame current{};
  Frame previous{};
  PartialUpdatePlanner planner;
  for (auto [width, pages] : dimensions) {
    for (size_t index = 0; index < width * pages; index++) {
      current[index] = 0xA5;
      ASSERT_TRUE(planner.build(current.data(), previous.data(), width, pages));
      verify_plan(current, previous, width, pages, planner);
      EXPECT_EQ(planner.get_wire_clocks(), 99U);
      current[index] = 0;
    }
  }
}

TEST(SSD1306Planner, ExhaustiveSixteenColumnPatterns) {
  Frame current{};
  Frame previous{};
  PartialUpdatePlanner planner;
  for (uint32_t mask = 0; mask < 65536; mask++) {
    for (unsigned x = 0; x < 16; x++) {
      current[x] = (mask >> x) & 1U;
    }
    ASSERT_TRUE(planner.build(current.data(), previous.data(), 16, 1));
    verify_plan(current, previous, 16, 1, planner);
  }
}

TEST(SSD1306Planner, GapBoundaryAndChunkRounding) {
  EXPECT_EQ(PartialUpdatePlanner::MAX_ALWAYS_MERGE_GAP, 6U);
  Frame current{};
  Frame previous{};
  PartialUpdatePlanner planner;
  for (unsigned gap = 0; gap <= 24; gap++) {
    for (unsigned left = 1; left <= 16; left++) {
      current.fill(0);
      std::fill_n(current.data(), left, 1);
      current[left + gap] = 1;
      ASSERT_TRUE(planner.build(current.data(), previous.data(), 128, 1));
      verify_plan(current, previous, 128, 1, planner);
      if (gap <= 6) {
        EXPECT_EQ(planner.get_region_count(), 1U);
      }
      const Region joined{0, static_cast<uint8_t>(left + gap), 0, 0};
      const Region first{0, static_cast<uint8_t>(left - 1), 0, 0};
      const Region second{static_cast<uint8_t>(left + gap), static_cast<uint8_t>(left + gap), 0, 0};
      EXPECT_EQ(planner.get_wire_clocks(),
                std::min(independent_cost(joined), independent_cost(first) + independent_cost(second)));
    }
  }
}

TEST(SSD1306Planner, OverflowStillCoversLaterPagesAndLastByte) {
  Frame current{};
  Frame previous{};
  current[0] = 1;
  current[120] = 1;
  current[1023] = 1;
  PartialUpdatePlanner planner;
  for (size_t capacity : {0U, 1U, 2U}) {
    ASSERT_TRUE(planner.build(current.data(), previous.data(), 128, 8, capacity));
    EXPECT_TRUE(planner.used_fallback());
    verify_plan(current, previous, 128, 8, planner);
    EXPECT_EQ(planner.get_region(0).max_x, 127);
    EXPECT_EQ(planner.get_region(0).max_page, 7);
  }
}

TEST(SSD1306Planner, ReconstructionPreservesOrderAtExactCapacity) {
  Frame current{};
  Frame previous{};
  for (size_t x : {0U, 2U, 40U, 42U, 96U, 98U}) {
    current[x] = 1;
  }
  PartialUpdatePlanner planner;
  ASSERT_TRUE(planner.build(current.data(), previous.data(), 128, 1, 3));
  ASSERT_EQ(planner.get_region_count(), 3U);
  EXPECT_FALSE(planner.used_fallback());
  expect_region(planner.get_region(0), Region{0, 2, 0, 0});
  expect_region(planner.get_region(1), Region{40, 42, 0, 0});
  expect_region(planner.get_region(2), Region{96, 98, 0, 0});
  verify_plan(current, previous, 128, 1, planner);
}

TEST(SSD1306Planner, ReconstructionAppendsAfterEarlierPages) {
  Frame current{};
  Frame previous{};
  for (size_t x : {0U, 2U, 40U, 42U}) {
    current[x] = 1;
  }
  for (size_t x : {80U, 82U, 120U, 122U}) {
    current[7 * 128 + x] = 1;
  }
  PartialUpdatePlanner planner;
  ASSERT_TRUE(planner.build(current.data(), previous.data(), 128, 8, 4));
  ASSERT_EQ(planner.get_region_count(), 4U);
  EXPECT_FALSE(planner.used_fallback());
  expect_region(planner.get_region(0), Region{0, 2, 0, 0});
  expect_region(planner.get_region(1), Region{40, 42, 0, 0});
  expect_region(planner.get_region(2), Region{80, 82, 7, 7});
  expect_region(planner.get_region(3), Region{120, 122, 7, 7});
  verify_plan(current, previous, 128, 8, planner);
}

TEST(SSD1306Planner, ReconstructionRejectsInsufficientCapacityBeforeWriting) {
  PagePlanner planner;
  const std::array<uint8_t, 1> first_run{3};
  const std::array<uint8_t, 4> runs{40, 42, 96, 98};
  planner.set_capacity(2);
  ASSERT_TRUE(planner.append_page(0, first_run.data(), first_run.data(), first_run.size()));
  EXPECT_FALSE(planner.append_page(1, runs.data(), runs.data(), runs.size()));
  ASSERT_EQ(planner.get_region_count(), 1U);
  expect_region(planner.get_region(0), Region{3, 3, 0, 0});
  for (size_t i = 1; i < PartialUpdatePlanner::MAX_PLANNED_REGIONS; i++) {
    expect_region(planner.get_region(i), Region{255, 255, 255, 255});
  }
  planner.set_capacity(3);
  ASSERT_TRUE(planner.append_page(1, runs.data(), runs.data(), runs.size()));
  ASSERT_EQ(planner.get_region_count(), 3U);
  expect_region(planner.get_region(0), Region{3, 3, 0, 0});
  expect_region(planner.get_region(1), Region{40, 42, 1, 1});
  expect_region(planner.get_region(2), Region{96, 98, 1, 1});
}

TEST(SSD1306Planner, ReconstructionOverflowCoversLaterPagesAndWorkspaceIsReusable) {
  Frame current{};
  Frame previous{};
  current[3] = 1;
  for (size_t x : {40U, 42U, 96U, 98U}) {
    current[128 + x] = 1;
  }
  current[1023] = 1;
  PartialUpdatePlanner planner;
  ASSERT_TRUE(planner.build(current.data(), previous.data(), 128, 8, 2));
  ASSERT_EQ(planner.get_region_count(), 1U);
  EXPECT_TRUE(planner.used_fallback());
  expect_region(planner.get_region(0), Region{3, 127, 0, 7});
  verify_plan(current, previous, 128, 8, planner);
  ASSERT_TRUE(planner.build(current.data(), previous.data(), 128, 8, 4));
  ASSERT_EQ(planner.get_region_count(), 4U);
  EXPECT_FALSE(planner.used_fallback());
  expect_region(planner.get_region(0), Region{3, 3, 0, 0});
  expect_region(planner.get_region(1), Region{40, 42, 1, 1});
  expect_region(planner.get_region(2), Region{96, 98, 1, 1});
  expect_region(planner.get_region(3), Region{127, 127, 7, 7});
  verify_plan(current, previous, 128, 8, planner);
}

TEST(SSD1306Planner, DenseCheckerboardFullFrameAndVerticalColumns) {
  Frame current{};
  Frame previous{};
  PartialUpdatePlanner planner;
  for (unsigned pattern = 0; pattern < 3; pattern++) {
    for (size_t i = 0; i < current.size(); i++) {
      current[i] = pattern == 0 ? 0xFF : ((pattern == 1 ? i % 2 == 0 : i % 128 == 7) ? 1 : 0);
    }
    ASSERT_TRUE(planner.build(current.data(), previous.data(), 128, 8));
    verify_plan(current, previous, 128, 8, planner);
  }
}

TEST(SSD1306Planner, RandomCoverageIncludingForcedOverflow) {
  // Fixed seed keeps coverage failures reproducible; this is not a security RNG.
  // NOLINTNEXTLINE(bugprone-random-generator-seed,cert-msc32-c,cert-msc51-cpp)
  std::mt19937 random(19802);
  const std::array<std::pair<uint16_t, uint8_t>, 6> dimensions{
      {{128, 4}, {128, 8}, {96, 2}, {64, 6}, {64, 4}, {72, 5}}};
  Frame current{};
  Frame previous{};
  PartialUpdatePlanner planner;
  for (auto [width, pages] : dimensions) {
    for (unsigned iteration = 0; iteration < 1000; iteration++) {
      for (size_t i = 0; i < width * pages; i++) {
        previous[i] = static_cast<uint8_t>(random());
        current[i] = previous[i];
        if (random() % 100 < 1 + iteration % 99) {
          current[i] ^= static_cast<uint8_t>(1 + random() % 255);
        }
      }
      const size_t capacity = iteration % 5 == 0 ? iteration % 4 : PartialUpdatePlanner::MAX_PLANNED_REGIONS;
      ASSERT_TRUE(planner.build(current.data(), previous.data(), width, pages, capacity));
      verify_plan(current, previous, width, pages, planner);
    }
  }
}

TEST(SSD1306Planner, InvalidInputDoesNotLeaveAConsumablePlan) {
  Frame current{};
  Frame previous{};
  PartialUpdatePlanner planner;
  current[0] = 1;
  ASSERT_TRUE(planner.build(current.data(), previous.data(), 128, 8));
  EXPECT_FALSE(planner.build(nullptr, previous.data(), 128, 8));
  EXPECT_EQ(planner.get_region_count(), 0U);
  EXPECT_FALSE(planner.build(current.data(), nullptr, 128, 8));
  EXPECT_FALSE(planner.build(current.data(), previous.data(), 0, 8));
  EXPECT_FALSE(planner.build(current.data(), previous.data(), 129, 8));
  EXPECT_FALSE(planner.build(current.data(), previous.data(), 128, 0));
  EXPECT_FALSE(planner.build(current.data(), previous.data(), 128, 9));
}
}  // namespace
}  // namespace esphome::ssd1306_i2c
