#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/nfc/ndef_record_uri.h"

namespace esphome::nfc::testing {

static std::string decode(const std::vector<uint8_t> &payload) { return NdefRecordUri(payload).get_payload(); }

static uint8_t encoded_prefix(const std::string &uri) { return NdefRecordUri(uri).get_encoded_payload()[0]; }

TEST(NdefRecordUri, DecodeExpandsPrefixCode) {
  EXPECT_EQ(decode({0x01, 'a', '.', 'b'}), "http://www.a.b");
  EXPECT_EQ(decode({0x04, 'a', '.', 'b'}), "https://a.b");
  EXPECT_EQ(decode({0x07, 'a'}), "ftp://anonymous:anonymous@a");
}

TEST(NdefRecordUri, DecodeAcceptsLastPrefixCode) { EXPECT_EQ(decode({0x23, 'x'}), "urn:nfc:x"); }

TEST(NdefRecordUri, DecodeIgnoresUnknownPrefixCode) {
  EXPECT_EQ(decode({0x00, 'x'}), "x");
  EXPECT_EQ(decode({0x24, 'x'}), "x");
}

TEST(NdefRecordUri, EncodePicksFirstMatchingPrefix) {
  EXPECT_EQ(encoded_prefix("https://www.a.b"), 0x02);
  EXPECT_EQ(encoded_prefix("http://a.b"), 0x03);
  EXPECT_EQ(encoded_prefix("urn:epc:id:x"), 0x13);
}

TEST(NdefRecordUri, EncodeStopsBeforeLastPrefixCode) {
  // urn:nfc: (0x23) is never chosen; the shorter urn: (0x13) matches first
  EXPECT_EQ(encoded_prefix("urn:nfc:x"), 0x13);
}

TEST(NdefRecordUri, EncodeStripsPrefix) {
  const std::vector<uint8_t> expected = {0x02, 'a', '.', 'b'};
  EXPECT_EQ(NdefRecordUri("https://www.a.b").get_encoded_payload(), expected);
}

TEST(NdefRecordUri, EncodeWithoutPrefix) {
  EXPECT_EQ(encoded_prefix("ht"), 0x00);
  const std::vector<uint8_t> expected = {0x00, 'x', 'y'};
  EXPECT_EQ(NdefRecordUri("xy").get_encoded_payload(), expected);
}

}  // namespace esphome::nfc::testing
