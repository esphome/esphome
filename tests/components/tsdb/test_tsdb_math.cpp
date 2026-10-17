#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "esphome/components/tsdb/tsdb_math.h"

namespace esphome::tsdb::testing {

// The capacity solver shrinks the raw estimate until the real file geometry fits.
TEST(TsdbMath, CapacityForBytesStaysWithinBudget) {
  for (uint32_t columns = 1; columns <= 8; columns++) {
    for (uint32_t stride : {32u, 190u, 380u}) {
      for (uint64_t bytes : {4096ULL, 16384ULL, 65536ULL, 262144ULL, 1048576ULL}) {
        const uint32_t records = tsdbmath::capacity_for_bytes(bytes, columns, stride);
        EXPECT_LE(tsdbmath::file_bytes_for(records, columns, stride), bytes)
            << "columns=" << columns << " stride=" << stride << " bytes=" << bytes;
      }
    }
  }
}

// A larger budget can never hold fewer records.
TEST(TsdbMath, CapacityForBytesIsMonotonic) {
  uint32_t previous = 0;
  for (uint64_t bytes = 4096; bytes <= 262144; bytes += 4096) {
    const uint32_t records = tsdbmath::capacity_for_bytes(bytes, 4);
    EXPECT_GE(records, previous) << "bytes=" << bytes;
    previous = records;
  }
}

TEST(TsdbMath, CapacityForBytesRejectsTooSmallBudget) {
  // Below the reserve the estimate is zero and the solver returns zero.
  EXPECT_EQ(tsdbmath::capacity_for_bytes(1024, 4), 0u);
  // A zero column count or stride cannot describe a record.
  EXPECT_EQ(tsdbmath::file_bytes_for(0, 4), 0u);
  EXPECT_EQ(tsdbmath::file_bytes_for(10, 0), 0u);
  EXPECT_EQ(tsdbmath::file_bytes_for(10, 4, 0), 0u);
}

TEST(TsdbMath, EncodeClampsAndReportsSaturation) {
  bool saturated = true;
  EXPECT_EQ(tsdbmath::encode(40000.0, 1.0, 0.0, &saturated), tsdbmath::RAW_MAX);
  EXPECT_TRUE(saturated);
  EXPECT_EQ(tsdbmath::encode(-40000.0, 1.0, 0.0, &saturated), tsdbmath::RAW_MIN);
  EXPECT_TRUE(saturated);
  // A huge scaled value can never masquerade as the unknown sentinel.
  EXPECT_EQ(tsdbmath::encode(1e30, 1.0, 0.0, &saturated), tsdbmath::RAW_MAX);
  EXPECT_NE(tsdbmath::encode(1e30, 1.0, 0.0, &saturated), tsdbmath::RAW_UNKNOWN);
}

TEST(TsdbMath, EncodeExactBoundariesAreNotSaturated) {
  bool saturated = true;
  EXPECT_EQ(tsdbmath::encode(32767.0, 1.0, 0.0, &saturated), tsdbmath::RAW_MAX);
  EXPECT_FALSE(saturated);
  EXPECT_EQ(tsdbmath::encode(-32767.0, 1.0, 0.0, &saturated), tsdbmath::RAW_MIN);
  EXPECT_FALSE(saturated);
  // -32768 would collide with the sentinel, so it is clamped to RAW_MIN instead.
  EXPECT_EQ(tsdbmath::encode(-32768.0, 1.0, 0.0, &saturated), tsdbmath::RAW_MIN);
  EXPECT_TRUE(saturated);
}

TEST(TsdbMath, EncodeUnknownInputs) {
  bool saturated = true;
  EXPECT_EQ(tsdbmath::encode(NAN, 1.0, 0.0, &saturated), tsdbmath::RAW_UNKNOWN);
  EXPECT_FALSE(saturated);
  EXPECT_EQ(tsdbmath::encode(1.0, 0.0), tsdbmath::RAW_UNKNOWN);
  EXPECT_EQ(tsdbmath::encode(INFINITY, 1.0), tsdbmath::RAW_UNKNOWN);
}

TEST(TsdbMath, EncodeDecodeRoundTrip) {
  for (double value : {-40.0, -1.5, 0.0, 21.4, 99.9}) {
    const int16_t raw = tsdbmath::encode(value, 10.0);
    EXPECT_NEAR(tsdbmath::decode(raw, 10.0), value, 0.05) << "value=" << value;
  }
  // Offset is applied on encode and removed on decode.
  for (double value : {-40.0, 21.4}) {
    const int16_t raw = tsdbmath::encode(value, 100.0, -40.0);
    EXPECT_NEAR(tsdbmath::decode(raw, 100.0, -40.0), value, 0.005) << "value=" << value;
  }
}

TEST(TsdbMath, DecodeUnknownAndInvalidScale) {
  EXPECT_TRUE(std::isnan(tsdbmath::decode(tsdbmath::RAW_UNKNOWN, 1.0)));
  EXPECT_TRUE(std::isnan(tsdbmath::decode(5, 0.0)));
  EXPECT_DOUBLE_EQ(tsdbmath::decode(123, 1.0), 123.0);
}

}  // namespace esphome::tsdb::testing
