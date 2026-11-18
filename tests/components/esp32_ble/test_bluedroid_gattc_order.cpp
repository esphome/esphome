#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "esphome/components/esp32_ble/bluedroid_gattc_order.h"

namespace esphome::esp32_ble::testing {

namespace {

void push_handle(std::vector<uint8_t> &pdu, uint16_t handle) {
  pdu.insert(pdu.end(), {uint8_t(handle), uint8_t(handle >> 8)});
}

// Read By Type for characteristic discovery: 7 byte records of handle, properties, value handle, UUID16
std::vector<uint8_t> characteristics(std::initializer_list<uint16_t> handles) {
  std::vector<uint8_t> pdu{7};
  for (uint16_t h : handles) {
    push_handle(pdu, h);
    pdu.push_back(0x02);
    push_handle(pdu, h + 1);
    pdu.insert(pdu.end(), {uint8_t(h), 0x2a});
  }
  return pdu;
}

// Find Information: format byte, then handle and UUID16 (format 1) or handle and UUID128 (format 2)
std::vector<uint8_t> descriptors(uint8_t format, std::initializer_list<uint16_t> handles) {
  std::vector<uint8_t> pdu{format};
  for (uint16_t h : handles) {
    push_handle(pdu, h);
    pdu.insert(pdu.end(), format == 1 ? 2 : 16, uint8_t(h));
  }
  return pdu;
}

// Find By Type Value: found handle and group end handle, no header
std::vector<uint8_t> service_ranges(std::initializer_list<uint16_t> starts) {
  std::vector<uint8_t> pdu;
  for (uint16_t s : starts) {
    push_handle(pdu, s);
    push_handle(pdu, s + 0x0f);
  }
  return pdu;
}

void sort(uint8_t op_code, std::vector<uint8_t> &pdu) { sort_att_response_by_handle(op_code, pdu.data(), pdu.size()); }

}  // namespace

// The records the walking pad in esphome#22518 sent for its Device Information service
TEST(SortAttResponseByHandle, OutOfOrderRecordsAreSortedWhole) {
  auto pdu = characteristics({0x0109, 0x0107, 0x010B});
  sort(ATT_READ_BY_TYPE_RSP, pdu);
  EXPECT_EQ(pdu, characteristics({0x0107, 0x0109, 0x010B}));
}

TEST(SortAttResponseByHandle, ReverseOrderIsSorted) {
  auto pdu = characteristics({0x0300, 0x0200, 0x0100, 0x0010});
  sort(ATT_READ_BY_TYPE_RSP, pdu);
  EXPECT_EQ(pdu, characteristics({0x0010, 0x0100, 0x0200, 0x0300}));
}

TEST(SortAttResponseByHandle, OrderedAndSingleRecordsAreUntouched) {
  auto ordered = characteristics({0x0010, 0x0012, 0x0014});
  auto single = characteristics({0x0042});
  const auto ordered_before = ordered;
  const auto single_before = single;
  sort(ATT_READ_BY_TYPE_RSP, ordered);
  sort(ATT_READ_BY_TYPE_RSP, single);
  EXPECT_EQ(ordered, ordered_before);
  EXPECT_EQ(single, single_before);
}

// Bluedroid ignores a partial last record, so it must stay where it is
TEST(SortAttResponseByHandle, PartialTailStaysInPlace) {
  auto pdu = characteristics({0x0109, 0x0107});
  pdu.insert(pdu.end(), {0x01, 0x01, 0x02});
  sort(ATT_READ_BY_TYPE_RSP, pdu);
  auto expected = characteristics({0x0107, 0x0109});
  expected.insert(expected.end(), {0x01, 0x01, 0x02});
  EXPECT_EQ(pdu, expected);
}

// A record too short to hold a handle, an unknown Find Information format or an empty PDU is left for Bluedroid
TEST(SortAttResponseByHandle, MalformedResponsesAreUntouched) {
  std::vector<uint8_t> short_records{1, 0x09, 0x07};
  auto bad_format = descriptors(1, {0x0109, 0x0107});
  bad_format[0] = 3;
  const auto short_before = short_records;
  const auto format_before = bad_format;
  sort(ATT_READ_BY_TYPE_RSP, short_records);
  sort(ATT_FIND_INFO_RSP, bad_format);
  EXPECT_EQ(short_records, short_before);
  EXPECT_EQ(bad_format, format_before);
  sort_att_response_by_handle(ATT_READ_BY_TYPE_RSP, nullptr, 0);
}

// Read By Group Type (service discovery) records start with the service's start handle too
TEST(SortAttResponseByHandle, GroupRecordsSortByStartHandle) {
  std::vector<uint8_t> pdu{6, 0x20, 0x00, 0x2f, 0x00, 0x0d, 0x18, 0x01, 0x00, 0x1f, 0x00, 0x00, 0x18};
  sort(ATT_READ_BY_GRP_TYPE_RSP, pdu);
  EXPECT_EQ(pdu, (std::vector<uint8_t>{6, 0x01, 0x00, 0x1f, 0x00, 0x00, 0x18, 0x20, 0x00, 0x2f, 0x00, 0x0d, 0x18}));
}

// Descriptor discovery uses Find Information, which ESP-IDF 5.5.6 checks the same way
TEST(SortAttResponseByHandle, FindInformationSortsBothFormats) {
  auto uuid16 = descriptors(1, {0x010A, 0x0108, 0x0109});
  auto uuid128 = descriptors(2, {0x0122, 0x0121});
  sort(ATT_FIND_INFO_RSP, uuid16);
  sort(ATT_FIND_INFO_RSP, uuid128);
  EXPECT_EQ(uuid16, descriptors(1, {0x0108, 0x0109, 0x010A}));
  EXPECT_EQ(uuid128, descriptors(2, {0x0121, 0x0122}));
}

// Service discovery by UUID uses Find By Type Value, which has no header byte
TEST(SortAttResponseByHandle, FindByTypeValueSortsServiceRanges) {
  auto pdu = service_ranges({0x0030, 0x0010, 0x0020});
  sort(ATT_FIND_BY_TYPE_VALUE_RSP, pdu);
  EXPECT_EQ(pdu, service_ranges({0x0010, 0x0020, 0x0030}));
}

// No ATT_MTU allows more than 516 bytes after the opcode; Bluedroid rejects longer bodies, so they are not sorted
TEST(SortAttResponseByHandle, OversizedBodiesAreLeftForBluedroid) {
  std::vector<uint8_t> largest{2};
  for (uint16_t h = 257; h > 0; h--)
    push_handle(largest, h);
  largest.push_back(0x00);  // partial last record
  ASSERT_EQ(largest.size(), ATT_MAX_MTU - 1);
  auto oversized = largest;
  oversized.push_back(0x00);
  const auto oversized_before = oversized;
  sort(ATT_READ_BY_TYPE_RSP, largest);
  sort(ATT_READ_BY_TYPE_RSP, oversized);
  EXPECT_EQ(encode_uint16(largest[2], largest[1]), 1);
  EXPECT_EQ(oversized, oversized_before);
}

TEST(SortAttResponseByHandle, OtherResponsesAreUntouched) {
  auto pdu = characteristics({0x0109, 0x0107});
  const auto before = pdu;
  sort(0x0B, pdu);  // Read Response
  EXPECT_EQ(pdu, before);
}

}  // namespace esphome::esp32_ble::testing
