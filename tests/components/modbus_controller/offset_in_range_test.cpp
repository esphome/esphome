#include <gtest/gtest.h>

#include <cstdint>
#include <span>

#include "esphome/components/modbus_controller/modbus_controller.h"

namespace esphome::modbus_controller::testing {

namespace {

// Minimal concrete SensorItem so the position/address accessors can be exercised directly.
class TestSensorItem : public SensorItem {
 public:
  void parse_and_publish(uint16_t /*base_address*/, std::span<const uint8_t> /*data*/) override {}
};

// Builds an item the way a platform constructor does, before ranges are built.
TestSensorItem make_item(modbus::EntityType type, uint16_t address, uint8_t offset) {
  TestSensorItem item;
  item.register_type = type;
  item.set_address(address);
  item.set_offset_from_start_address(offset);
  return item;
}

}  // namespace

// A freshly constructed item is already usable: its resolved position is the offset as configured and
// its range base is its own address, which is what an item that never gets polled relies on.
TEST(SensorItemPosition, ConstructionSeedsResolvedPositionAndRangeBase) {
  auto item = make_item(modbus::EntityType::HOLDING, 0x9001, 4);
  EXPECT_EQ(item.offset_from_start_address, 4);
  EXPECT_EQ(item.offset, 4);
  EXPECT_EQ(item.range_start_address, 0x9001);
  EXPECT_EQ(item.offset_in_range(0x9001), 4u);
}

// offset_in_range() reports the position resolved while the ranges were built. It is relative to the
// range's first register, so it does not re-derive anything from base_address.
TEST(SensorItemPosition, OffsetInRangeReportsResolvedPosition) {
  auto item = make_item(modbus::EntityType::HOLDING, 0x9003, 0);
  item.range_start_address = 0x9001;  // grouped into a range starting two registers earlier
  item.offset = 4;                    // resolved: two 2-byte registers ahead of it
  EXPECT_EQ(item.offset_in_range(0x9001), 4u);
}

// A write lands on the register the sensor reads from: the range's first register plus the resolved
// byte offset converted to registers.
TEST(SensorItemPosition, WriteAddressForRegisters) {
  auto item = make_item(modbus::EntityType::HOLDING, 0x9003, 0);
  item.range_start_address = 0x9001;
  item.offset = 4;
  EXPECT_EQ(item.write_address(), 0x9003);
}

// Coils index bits, so the resolved offset is a bit count and is added to the range base directly.
TEST(SensorItemPosition, WriteAddressForCoils) {
  auto item = make_item(modbus::EntityType::COIL, 0x15, 0);
  item.range_start_address = 0x10;
  item.offset = 5;
  EXPECT_EQ(item.write_address(), 0x15);
  EXPECT_TRUE(item.addresses_bits());
}

// An item that is never polled keeps the range base its constructor set, so its write address is still
// its own address plus its configured offset - a switch with assumed_state, or an output.
TEST(SensorItemPosition, WriteAddressWithoutAGroupedRange) {
  auto item = make_item(modbus::EntityType::HOLDING, 0x9010, 2);
  EXPECT_EQ(item.write_address(), 0x9011);
}

// A sensor re-using a register after one with a non-zero offset resolves past that offset, and its
// write address follows the same position - the behaviour releases before the range rework had.
TEST(SensorItemPosition, ReUseChainWriteAddressFollowsResolvedPosition) {
  auto item = make_item(modbus::EntityType::HOLDING, 0x9001, 4);
  item.range_start_address = 0x9001;
  item.offset = 6;  // 4 configured, plus the 2 the previous sensor on this register resolved to
  EXPECT_EQ(item.offset_in_range(0x9001), 6u);
  EXPECT_EQ(item.write_address(), 0x9004);
}

// Registers address 16-bit words; only coils and discrete inputs address bits.
TEST(SensorItemPosition, AddressesBitsOnlyForCoilAndDiscreteInput) {
  EXPECT_FALSE(make_item(modbus::EntityType::HOLDING, 0, 0).addresses_bits());
  EXPECT_FALSE(make_item(modbus::EntityType::READ, 0, 0).addresses_bits());
  EXPECT_TRUE(make_item(modbus::EntityType::COIL, 0, 0).addresses_bits());
  EXPECT_TRUE(make_item(modbus::EntityType::DISCRETE_INPUT, 0, 0).addresses_bits());
}

}  // namespace esphome::modbus_controller::testing
