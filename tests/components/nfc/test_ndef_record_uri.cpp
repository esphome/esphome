#include <gtest/gtest.h>

#include <iterator>
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

// URI identifier codes from the NFC Forum URI Record Type Definition, kept separate from the
// component's table so a reordered entry fails here
static constexpr const char *SPEC_PREFIXES[] = {"",
                                                "http://www.",
                                                "https://www.",
                                                "http://",
                                                "https://",
                                                "tel:",
                                                "mailto:",
                                                "ftp://anonymous:anonymous@",
                                                "ftp://ftp.",
                                                "ftps://",
                                                "sftp://",
                                                "smb://",
                                                "nfs://",
                                                "ftp://",
                                                "dav://",
                                                "news:",
                                                "telnet://",
                                                "imap:",
                                                "rtsp://",
                                                "urn:",
                                                "pop:",
                                                "sip:",
                                                "sips:",
                                                "tftp:",
                                                "btspp://",
                                                "btl2cap://",
                                                "btgoep://",
                                                "tcpobex://",
                                                "irdaobex://",
                                                "file://",
                                                "urn:epc:id:",
                                                "urn:epc:tag:",
                                                "urn:epc:pat:",
                                                "urn:epc:raw:",
                                                "urn:epc:",
                                                "urn:nfc:"};

TEST(NdefRecordUri, DecodeEveryPrefixCode) {
  for (size_t code = 1; code < std::size(SPEC_PREFIXES); code++)
    EXPECT_EQ(decode({static_cast<uint8_t>(code), 'x'}), std::string(SPEC_PREFIXES[code]) + "x") << "code " << code;
}

TEST(NdefRecordUri, DecodeAcceptsLastPrefixCode) { EXPECT_EQ(decode({0x23, 'x'}), "urn:nfc:x"); }

TEST(NdefRecordUri, DecodeIgnoresUnknownPrefixCode) {
  EXPECT_EQ(decode({0x00, 'x'}), "x");
  EXPECT_EQ(decode({0x24, 'x'}), "x");
}

// Encode takes the first prefix in table order that matches, so codes 0x1E to 0x23 are never written;
// these tests pin that current behaviour, not the spec's longest match
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
