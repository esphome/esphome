#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/nfc/ndef_message.h"

namespace esphome::nfc::testing {

static size_t record_count(const std::vector<uint8_t> &data) { return NdefMessage(data).get_records().size(); }

// Header: MB|ME|SR|TNF=well-known, type length 1, payload length 4, type "T", payload (status, "en", "x")
TEST(NdefMessage, ParsesShortTextRecord) {
  NdefMessage message(std::vector<uint8_t>{0xD1, 0x01, 0x04, 'T', 0x02, 'e', 'n', 'x'});
  ASSERT_EQ(message.get_records().size(), 1u);
  EXPECT_EQ(message.get_records()[0]->get_type(), "T");
  EXPECT_EQ(message.get_records()[0]->get_payload(), "x");
}

TEST(NdefMessage, ParsesRecordWithId) {
  NdefMessage message(std::vector<uint8_t>{0xD9, 0x01, 0x04, 0x02, 'T', 'i', 'd', 0x02, 'e', 'n', 'x'});
  ASSERT_EQ(message.get_records().size(), 1u);
  EXPECT_EQ(message.get_records()[0]->get_type(), "T");
  EXPECT_EQ(message.get_records()[0]->get_id(), "id");
  EXPECT_EQ(message.get_records()[0]->get_payload(), "x");
}

TEST(NdefMessage, RejectsTypeLengthPastEnd) {
  EXPECT_EQ(record_count({0xD1, 0xFF, 0x00, 'T'}), 0u);
  EXPECT_EQ(record_count({0xD1, 0x02, 0x00, 'T'}), 0u);
}

TEST(NdefMessage, RejectsIdLengthPastEnd) {
  EXPECT_EQ(record_count({0xD9, 0x01, 0x00, 0xFF, 'T'}), 0u);
  EXPECT_EQ(record_count({0xD9, 0x01, 0x00, 0x02, 'T', 'i'}), 0u);
}

TEST(NdefMessage, RejectsPayloadLengthPastEnd) { EXPECT_EQ(record_count({0xD1, 0x01, 0x05, 'T', 'x'}), 0u); }

// Long record with a 4-byte payload length that would wrap a 32-bit pointer
TEST(NdefMessage, RejectsHugePayloadLength) {
  EXPECT_EQ(record_count({0xC1, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 'T', 'x'}), 0u);
}

TEST(NdefMessage, RejectsTruncatedHeader) {
  EXPECT_EQ(record_count({0xD1}), 0u);
  EXPECT_EQ(record_count({0xD1, 0x01}), 0u);
}

}  // namespace esphome::nfc::testing
