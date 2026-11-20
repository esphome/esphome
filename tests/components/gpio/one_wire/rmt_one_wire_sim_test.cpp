// Tests of the simulated bus itself. They need no engine, so they prove that the model the other
// tests rely on behaves like a 1-Wire bus, independent of any implementation.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "../common.h"

namespace esphome::gpio::rmt_one_wire::testing {

namespace {

constexpr Symbol SIM_RESET{500, 0, 500, 1};
constexpr Symbol SIM_READ{4, 0, 76, 1};
constexpr Symbol SIM_WRITE_1{6, 0, 74, 1};
constexpr Symbol SIM_WRITE_0{65, 0, 15, 1};

void expect_symbol(const Symbol &symbol, uint16_t low, uint16_t high, uint16_t end_level) {
  EXPECT_EQ(symbol.duration0, low);
  EXPECT_EQ(symbol.level0, 0);
  EXPECT_EQ(symbol.duration1, high);
  EXPECT_EQ(symbol.level1, end_level);
}

void write_byte(SimBus &bus, uint8_t value) {
  std::vector<Phase> phases;
  for (int bit = 0; bit < 8; bit++)
    bus.drive((value >> bit) & 1 ? SIM_WRITE_1 : SIM_WRITE_0, &phases);
}

}  // namespace

TEST(RmtOneWireSim, ResetWithDevice) {
  SimBus bus;
  bus.devices.emplace_back(ROM_A);
  std::vector<Phase> phases;
  bus.drive(SIM_RESET, &phases);
  uint64_t end = 0;
  const auto capture = build_capture(phases, 600, &end);
  ASSERT_EQ(capture.size(), 2u);
  expect_symbol(capture[0], 503, 27, 1);
  expect_symbol(capture[1], 120, 0, 1);
  EXPECT_EQ(end, 1250u);  // inside the 1100-1400 us of section 4.3
}

TEST(RmtOneWireSim, ResetWithoutDevice) {
  SimBus bus;
  std::vector<Phase> phases;
  bus.drive(SIM_RESET, &phases);
  uint64_t end = 0;
  const auto capture = build_capture(phases, 600, &end);
  ASSERT_EQ(capture.size(), 1u);
  expect_symbol(capture[0], 503, 0, 1);
  EXPECT_EQ(end, 1103u);
}

TEST(RmtOneWireSim, TwoDevicesOverlapToOnePresence) {
  SimBus bus;
  auto &first = bus.devices.emplace_back(ROM_A);
  auto &second = bus.devices.emplace_back(ROM_B);
  first.presence_wait_us = 20;
  first.presence_low_us = 100;
  second.presence_wait_us = 60;
  second.presence_low_us = 200;
  std::vector<Phase> phases;
  bus.drive(SIM_RESET, &phases);
  uint64_t end = 0;
  const auto capture = build_capture(phases, 600, &end);
  ASSERT_EQ(capture.size(), 2u);
  expect_symbol(capture[0], 503, 17, 1);
  expect_symbol(capture[1], 240, 0, 1);
}

TEST(RmtOneWireSim, IdleBelowResetLowEndsInsideThePulse) {
  // The bug of spec v1: with 320 us idle the capture ends on Low, before any presence
  SimBus bus;
  bus.devices.emplace_back(ROM_A);
  std::vector<Phase> phases;
  bus.drive(SIM_RESET, &phases);
  uint64_t end = 0;
  const auto capture = build_capture(phases, 320, &end);
  ASSERT_EQ(capture.size(), 1u);
  expect_symbol(capture[0], 0, STALE_DURATION1, 1);
  EXPECT_EQ(end, 320u);
}

TEST(RmtOneWireSim, ShortResetIsNotSeen) {
  SimBus bus;
  bus.devices.emplace_back(ROM_A);
  std::vector<Phase> phases;
  bus.drive(Symbol{470, 0, 500, 1}, &phases);
  uint64_t end = 0;
  const auto capture = build_capture(phases, 600, &end);
  ASSERT_EQ(capture.size(), 1u) << "no presence";
}

TEST(RmtOneWireSim, ReadSlotsOfASelectedDevice) {
  SimBus bus;
  bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{0b01101001});
  std::vector<Phase> phases;
  bus.drive(SIM_RESET, &phases);
  write_byte(bus, 0xCC);
  phases.clear();
  for (int i = 0; i < 8; i++)
    bus.drive(SIM_READ, &phases);
  uint64_t end = 0;
  const auto capture = build_capture(phases, 100, &end);
  ASSERT_EQ(capture.size(), 8u);
  for (int i = 0; i < 8; i++) {
    const bool bit = (0b01101001 >> i) & 1;
    EXPECT_EQ(capture[i].duration0 >= 13, !bit) << "slot " << i;
    if (i < 7) {
      EXPECT_EQ(capture[i].duration0 + capture[i].duration1, 80) << "slot " << i;
    }
  }
  EXPECT_EQ(capture[7].duration1, 0);
  EXPECT_EQ(capture[7].level1, 1);
}

TEST(RmtOneWireSim, MatchRomSelectsOnlyTheAddressedDevice) {
  SimBus bus;
  bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{0xFF});
  bus.devices.emplace_back(ROM_B, std::vector<uint8_t>{0x00});
  std::vector<Phase> phases;
  bus.drive(SIM_RESET, &phases);
  write_byte(bus, 0x55);
  for (int i = 0; i < 8; i++)
    write_byte(bus, static_cast<uint8_t>(ROM_A >> (8 * i)));
  for (int i = 0; i < 8; i++) {
    bus.drive(SIM_READ, &phases);
    EXPECT_TRUE(bus.last_slot_read()) << "the other device must stay quiet";
  }
}

TEST(RmtOneWireSim, DevicesAreSilentWithoutReset) {
  SimBus bus;
  bus.devices.emplace_back(ROM_A, std::vector<uint8_t>{0x00});
  std::vector<Phase> phases;
  for (int i = 0; i < 16; i++) {
    bus.drive(SIM_READ, &phases);
    EXPECT_TRUE(bus.last_slot_read());
  }
}

TEST(RmtOneWireSim, SearchSlotsOfTwoDevices) {
  // Bit 8 is the first that differs
  SimBus bus;
  bus.devices.emplace_back(uint64_t{0x0028});
  bus.devices.emplace_back(uint64_t{0x0128});
  std::vector<Phase> phases;
  bus.drive(SIM_RESET, &phases);
  write_byte(bus, 0xF0);
  for (int bit = 0; bit < 8; bit++) {
    const bool rom_bit = (0x28 >> bit) & 1;
    bus.drive(SIM_READ, &phases);
    EXPECT_EQ(bus.last_slot_read(), rom_bit) << "bit " << bit;
    bus.drive(SIM_READ, &phases);
    EXPECT_EQ(bus.last_slot_read(), !rom_bit) << "complement " << bit;
    bus.drive(rom_bit ? SIM_WRITE_1 : SIM_WRITE_0, &phases);
  }
  // Conflict: both read as 0
  bus.drive(SIM_READ, &phases);
  EXPECT_FALSE(bus.last_slot_read());
  bus.drive(SIM_READ, &phases);
  EXPECT_FALSE(bus.last_slot_read());
  // Taking the 1 branch drops the first device
  bus.drive(SIM_WRITE_1, &phases);
  bus.drive(SIM_READ, &phases);
  EXPECT_FALSE(bus.last_slot_read()) << "bit 9 of the remaining device is 0";
  bus.drive(SIM_READ, &phases);
  EXPECT_TRUE(bus.last_slot_read());
}

TEST(RmtOneWireSim, CaptureEndsWhenAHighPhaseReachesIdle) {
  std::vector<Phase> phases{{true, 4}, {false, 76}, {true, 4}, {false, 76}};
  uint64_t end = 0;
  const auto too_short = build_capture(phases, 50, &end);
  ASSERT_EQ(too_short.size(), 1u) << "the following edges are not recorded";
  expect_symbol(too_short[0], 4, 0, 1);
  EXPECT_EQ(end, 54u);
}

TEST(RmtOneWireSim, CaptureEndsWhenALowPhaseReachesIdle) {
  // S2 of the S3 probe: two slots, then the line stays Low
  std::vector<Phase> phases{{true, 4}, {false, 76}, {true, 4}, {false, 76}, {true, 2000}};
  uint64_t end = 0;
  const auto capture = build_capture(phases, 100, &end);
  ASSERT_EQ(capture.size(), 3u);
  expect_symbol(capture[0], 4, 76, 1);
  expect_symbol(capture[1], 4, 76, 1);
  expect_symbol(capture[2], 0, STALE_DURATION1, 1);
  EXPECT_EQ(end, 260u);
}

TEST(RmtOneWireSim, ClassificationFollowsSection3) {
  EXPECT_EQ(classify_slot(SIM_READ), Slot::READ);
  EXPECT_EQ(classify_slot(SIM_WRITE_1), Slot::WRITE_1);
  EXPECT_EQ(classify_slot(SIM_WRITE_0), Slot::WRITE_0);
  EXPECT_EQ(classify_slot(Symbol{6, 1, 74, 0}), Slot::BAD);
  EXPECT_EQ(classify_slot(Symbol{30, 0, 50, 1}), Slot::BAD);
  EXPECT_EQ(classify_slot(Symbol{130, 0, 10, 1}), Slot::BAD);
}

}  // namespace esphome::gpio::rmt_one_wire::testing
