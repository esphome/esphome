#include <gtest/gtest.h>

#include <string>

#include "esphome/components/canopen/od.h"

namespace esphome::canopen::testing {

// upload a whole VAR through CANopenNode's OD interface, as the SDO server does
static std::string read_var(ObjectDictionary &od, uint16_t index, OD_attr_t *attr = nullptr) {
  OD_IO_t io;
  EXPECT_EQ(OD_getSub(od.find(index), 0, &io, false), ODR_OK);
  if (attr != nullptr)
    *attr = io.stream.attribute;
  char buf[64];
  OD_size_t n = 0;
  EXPECT_EQ(io.read(&io.stream, buf, sizeof(buf), &n), ODR_OK);
  return std::string(buf, n);
}

TEST(CanopenOD, Strings) {
  static const char NAME[] = "canopen test node";
  static const char EMPTY[] = "";
  ObjectDictionary od;
  // added out of order: finalize() sorts
  od.add_string(0x100A, "2026.11.0");
  od.add_string(0x1008, NAME);
  od.add_string(0x1009, EMPTY);
  od.finalize();

  OD_attr_t attr = 0;
  EXPECT_EQ(read_var(od, 0x1008, &attr), NAME);
  // read only, no ODA_STR (size known up front) and no ODA_MB (no byte order)
  EXPECT_EQ(attr, ODA_SDO_R);
  EXPECT_EQ(read_var(od, 0x100A), "2026.11.0");
  // an empty string is one zero byte: CANopenNode aborts zero-length uploads
  EXPECT_EQ(read_var(od, 0x1009), std::string(1, '\0'));
  EXPECT_EQ(od.find(0x1007), nullptr);
}

TEST(CanopenOD, Record) {
  uint8_t highest = 2;
  uint32_t a = 0x11223344, b = 0;
  ObjectDictionary od;
  od.add_record(0x1200, {{0, ODA_SDO_R, &highest, 1}, {1, ODA_SDO_R, &a, 4}, {2, ODA_SDO_RW, &b, 4}});
  od.finalize();

  OD_entry_t *entry = od.find(0x1200);
  ASSERT_NE(entry, nullptr);
  uint32_t value = 0;
  EXPECT_EQ(OD_get_u32(entry, 1, &value, true), ODR_OK);
  EXPECT_EQ(value, 0x11223344u);
  // CANopenNode writes the default COB-IDs into 0x1200 this way (original data, no attribute check)
  EXPECT_EQ(OD_set_u32(entry, 2, 0x5FF, true), ODR_OK);
  EXPECT_EQ(b, 0x5FFu);
  uint8_t count = 0;
  EXPECT_EQ(OD_get_u8(entry, 0, &count, true), ODR_OK);
  EXPECT_EQ(count, 2);
}

}  // namespace esphome::canopen::testing
