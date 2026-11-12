#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/esp32_ble/bluedroid_gattc_order.h"

namespace esphome::esp32_ble::testing {

namespace {

// A Read By Type response for characteristic discovery: 7 byte records of handle, properties, value handle, UUID16
std::vector<uint8_t> characteristics(std::initializer_list<uint16_t> handles) {
  std::vector<uint8_t> pdu{7};
  for (uint16_t h : handles) {
    const uint16_t value = h + 1;
    pdu.insert(pdu.end(), {uint8_t(h), uint8_t(h >> 8), 0x02, uint8_t(value), uint8_t(value >> 8), uint8_t(h), 0x2a});
  }
  return pdu;
}

void sort(std::vector<uint8_t> &pdu) { sort_att_records_by_handle(pdu.data(), pdu.size()); }

}  // namespace

// The records the walking pad in esphome#22518 sent for its Device Information service
TEST(SortAttRecordsByHandle, OutOfOrderRecordsAreSortedWhole) {
  auto pdu = characteristics({0x0109, 0x0107, 0x010B});
  sort(pdu);
  EXPECT_EQ(pdu, characteristics({0x0107, 0x0109, 0x010B}));
}

TEST(SortAttRecordsByHandle, ReverseOrderIsSorted) {
  auto pdu = characteristics({0x0300, 0x0200, 0x0100, 0x0010});
  sort(pdu);
  EXPECT_EQ(pdu, characteristics({0x0010, 0x0100, 0x0200, 0x0300}));
}

TEST(SortAttRecordsByHandle, OrderedAndSingleRecordsAreUntouched) {
  auto ordered = characteristics({0x0010, 0x0012, 0x0014});
  auto single = characteristics({0x0042});
  const auto ordered_before = ordered;
  const auto single_before = single;
  sort(ordered);
  sort(single);
  EXPECT_EQ(ordered, ordered_before);
  EXPECT_EQ(single, single_before);
}

// Bluedroid ignores a partial last record, so it must stay where it is
TEST(SortAttRecordsByHandle, PartialTailStaysInPlace) {
  auto pdu = characteristics({0x0109, 0x0107});
  pdu.insert(pdu.end(), {0x01, 0x01, 0x02});
  sort(pdu);
  auto expected = characteristics({0x0107, 0x0109});
  expected.insert(expected.end(), {0x01, 0x01, 0x02});
  EXPECT_EQ(pdu, expected);
}

// A record too short to hold a handle, or an empty PDU, is left for Bluedroid to reject
TEST(SortAttRecordsByHandle, MalformedLengthsAreUntouched) {
  std::vector<uint8_t> short_records{1, 0x09, 0x07};
  const auto before = short_records;
  sort(short_records);
  EXPECT_EQ(short_records, before);
  sort_att_records_by_handle(nullptr, 0);
}

// Read By Group Type (service discovery) records start with the service's start handle too
TEST(SortAttRecordsByHandle, GroupRecordsSortByStartHandle) {
  std::vector<uint8_t> pdu{6, 0x20, 0x00, 0x2f, 0x00, 0x0d, 0x18, 0x01, 0x00, 0x1f, 0x00, 0x00, 0x18};
  sort(pdu);
  EXPECT_EQ(pdu, (std::vector<uint8_t>{6, 0x01, 0x00, 0x1f, 0x00, 0x00, 0x18, 0x20, 0x00, 0x2f, 0x00, 0x0d, 0x18}));
}

}  // namespace esphome::esp32_ble::testing
