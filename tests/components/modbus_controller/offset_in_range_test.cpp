#include <gtest/gtest.h>

#include <cstdint>
#include <span>

#include "esphome/components/modbus_controller/modbus_controller.h"

namespace esphome::modbus_controller::testing {

namespace {

// Minimal concrete SensorItem so offset_in_range() (the unit under test) can be exercised directly.
class TestSensorItem : public SensorItem {
 public:
  void parse_and_publish(uint16_t /*base_address*/, std::span<const uint8_t> /*data*/) override {}
};

TestSensorItem make_item(modbus::EntityType type, uint16_t start_address, uint8_t offset, uint16_t range_data_offset,
                         uint16_t shared_offset_bias = 0) {
  TestSensorItem item;
  item.register_type = type;
  item.start_address = start_address;
  item.offset = offset;
  item.range_data_offset = range_data_offset;
  item.shared_offset_bias = shared_offset_bias;
  return item;
}

}  // namespace

// A register sensor at the range start resolves to its configured byte offset - range_data_offset is 0.
TEST(OffsetInRange, RegisterAtRangeStart) {
  auto item = make_item(modbus::EntityType::HOLDING, 0x9001, 0, 0);
  EXPECT_EQ(item.offset_in_range(0x9001), 0u);
}

// A register sensor extending the range resolves to range_data_offset + offset. range_data_offset is the
// absolute byte position within the range response, so the address delta must NOT be added again (the
// old code did addr_delta*2 + (range_bytes - addr_delta*2), double-accounting was the latent bug).
TEST(OffsetInRange, RegisterExtendUsesAbsoluteOffset) {
  // Third register of a range that starts at 0x9001; two 2-byte registers precede it -> 4 bytes.
  auto item = make_item(modbus::EntityType::HOLDING, 0x9003, 0, 4);
  EXPECT_EQ(item.offset_in_range(0x9001), 4u);
}

// The absolute offset holds even when earlier registers returned MORE bytes than 2*register_count
// (wide response_size). The old delta correction underflowed here; the stored absolute value cannot.
TEST(OffsetInRange, RegisterWideResponseNoUnderflow) {
  // Previous register reported 3 bytes for a single 2-byte register, so this one starts at byte 3.
  auto item = make_item(modbus::EntityType::HOLDING, 0x9002, 0, 3);
  EXPECT_EQ(item.offset_in_range(0x9002 - 1), 3u);  // base one register earlier; delta must not shift it
}

// Coils/discrete inputs still resolve to a bit index: (start_address - base) + offset.
TEST(OffsetInRange, CoilUsesBitIndex) {
  auto item = make_item(modbus::EntityType::COIL, 0x0015, 5, 0);
  EXPECT_EQ(item.offset_in_range(0x0010), 0x0005u + 5u);  // 5 coils into the range, +5 configured offset
}

// A re-use chain (equal counts on one register) inherits each previous sensor's resolved offset via
// shared_offset_bias, so a chain configured 0 / 2 / 4 resolves to bytes 0 / 2 / 6 exactly as the
// pre-rewrite code did (it rewrote curr->offset += prev->offset).
TEST(OffsetInRange, ReUseChainAccumulatesSharedOffsetBias) {
  auto a = make_item(modbus::EntityType::HOLDING, 0x9001, 0, 0, 0);  // bias 0
  auto b = make_item(modbus::EntityType::HOLDING, 0x9001, 2, 0, 0);  // bias = a.bias + a.offset = 0
  auto c = make_item(modbus::EntityType::HOLDING, 0x9001, 4, 0, 2);  // bias = b.bias + b.offset = 2
  EXPECT_EQ(a.offset_in_range(0x9001), 0u);
  EXPECT_EQ(b.offset_in_range(0x9001), 2u);
  EXPECT_EQ(c.offset_in_range(0x9001), 6u);
}

// The shared-start fallback (non-mergeable sensors on one address) does not accumulate: each sensor
// resolves to exactly its configured offset, as the old fallback (which never rewrote fields) did.
TEST(OffsetInRange, SharedStartFallbackDoesNotAccumulate) {
  auto word = make_item(modbus::EntityType::HOLDING, 0x9001, 0, 0);
  auto dword = make_item(modbus::EntityType::HOLDING, 0x9001, 2, 0);
  EXPECT_EQ(word.offset_in_range(0x9001), 0u);
  EXPECT_EQ(dword.offset_in_range(0x9001), 2u);
}

// Write entities target start_address + (bias + offset) / 2, so a write in a re-use chain lands on the
// same register the sensor reads - matching the old rewritten-field arithmetic.
TEST(OffsetInRange, WriteAddressFollowsBias) {
  auto plain = make_item(modbus::EntityType::HOLDING, 0x9001, 2, 0, 0);
  EXPECT_EQ(plain.write_register_address(), 0x9002u);  // configured offset only
  auto chained = make_item(modbus::EntityType::HOLDING, 0x9001, 4, 0, 2);
  EXPECT_EQ(chained.write_register_address(), 0x9004u);  // (2 + 4) / 2 = 3 registers past start
}

}  // namespace esphome::modbus_controller::testing
