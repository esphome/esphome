#include <gtest/gtest.h>

#include <memory>

#include "esphome/components/nfc/ndef_message.h"
#include "esphome/components/nfc/ndef_record.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/pn532/pn532.h"

namespace esphome::pn532::testing {

/// Exposes the protected TLV parser so it can be driven directly with synthetic tag memory.
class TestablePN532 : public PN532 {
 public:
  using PN532::find_mifare_ultralight_ndef_;

  bool is_read_ready() override { return false; }
  bool write_data(const std::vector<uint8_t> &data) override { return false; }
  bool read_data(std::vector<uint8_t> &data, uint8_t len) override { return false; }
  bool read_response(uint8_t command, std::vector<uint8_t> &data) override { return false; }
};

// Builds a 16-byte page_3_to_6 buffer: 4 filler bytes for page 3 (unused by this function),
// followed by the given bytes starting at page 4, zero-padded out to the real 4-page read size.
std::vector<uint8_t> make_page_3_to_6(std::initializer_list<uint8_t> page4_onward) {
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x00};
  data.insert(data.end(), page4_onward);
  data.resize(16, 0x00);
  return data;
}

TEST(FindMifareUltralightNdef, ShortFormAtPage4Start) {
  TestablePN532 pn532;
  // 0x03 LEN -- message starts right after.
  auto data = make_page_3_to_6({0x03, 0x20});

  uint32_t message_length = 0;
  uint8_t message_start_index = 0;
  ASSERT_TRUE(pn532.find_mifare_ultralight_ndef_(data, message_length, message_start_index));
  EXPECT_EQ(message_length, 0x20u);
  EXPECT_EQ(message_start_index, 2);
}

TEST(FindMifareUltralightNdef, ShortFormAfterLeadingPadding) {
  TestablePN532 pn532;
  // A non-0x03 byte at page 4 (e.g. a lock/CC TLV) pushes the message TLV to offset 5.
  auto data = make_page_3_to_6({0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0x10});

  uint32_t message_length = 0;
  uint8_t message_start_index = 0;
  ASSERT_TRUE(pn532.find_mifare_ultralight_ndef_(data, message_length, message_start_index));
  EXPECT_EQ(message_length, 0x10u);
  EXPECT_EQ(message_start_index, 7);
}

// Regression test: prior to this fix, a >=255 byte NDEF message's TLV length was always read as
// a single byte, so the 0xFF long-form marker was mistaken for a length of 255 and the two real
// length bytes were mistaken for message content. That misalignment is what produced the
// "Corrupt record encountered; NdefMessage constructor aborting" error for large tags
TEST(FindMifareUltralightNdef, LongFormAtPage4Start) {
  TestablePN532 pn532;
  // 0x03 FF 01 2C -> long-form length = 0x012C = 300
  auto data = make_page_3_to_6({0x03, 0xFF, 0x01, 0x2C, 0xD1, 0x01, 0x0C, 0x55});

  uint32_t message_length = 0;
  uint8_t message_start_index = 0;
  ASSERT_TRUE(pn532.find_mifare_ultralight_ndef_(data, message_length, message_start_index));
  EXPECT_EQ(message_length, 300u);
  EXPECT_EQ(message_start_index, 4);
}

TEST(FindMifareUltralightNdef, LongFormAfterLeadingPadding) {
  TestablePN532 pn532;
  auto data = make_page_3_to_6({0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0x02, 0x00});

  uint32_t message_length = 0;
  uint8_t message_start_index = 0;
  ASSERT_TRUE(pn532.find_mifare_ultralight_ndef_(data, message_length, message_start_index));
  EXPECT_EQ(message_length, 0x0200u);  // 512
  EXPECT_EQ(message_start_index, 9);
}

TEST(FindMifareUltralightNdef, TooShortBufferIsRejected) {
  TestablePN532 pn532;
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x00, 0x03, 0x05};  // well under the size floor

  uint32_t message_length = 0;
  uint8_t message_start_index = 0;
  EXPECT_FALSE(pn532.find_mifare_ultralight_ndef_(data, message_length, message_start_index));
}

TEST(FindMifareUltralightNdef, NoTlvTagIsRejected) {
  TestablePN532 pn532;
  auto data = make_page_3_to_6({0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00});

  uint32_t message_length = 0;
  uint8_t message_start_index = 0;
  EXPECT_FALSE(pn532.find_mifare_ultralight_ndef_(data, message_length, message_start_index));
}

// Regression test: the entry size guard (page_3_to_6.size() > p4_offset + 6) is loose enough to admit
// buffers that are too short to hold the two long-form length bytes once the TLV is offset by leading
// padding (tlv_offset == 5). Before this fix, that case silently fell through to the short-form branch
// and reported a bogus 255-byte message instead of failing.
TEST(FindMifareUltralightNdef, LongFormMarkerWithMissingLengthBytesIsRejected) {
  TestablePN532 pn532;
  // Padding pushes the TLV to offset 5; 0x03 FF present, but the two length bytes are missing (size 11).
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0xFF};
  ASSERT_EQ(data.size(), 11u);

  uint32_t message_length = 0;
  uint8_t message_start_index = 0;
  EXPECT_FALSE(pn532.find_mifare_ultralight_ndef_(data, message_length, message_start_index));
}

/// End-to-end regression guard: builds a real 3-record NDEF message (URL, text, and a MIME
/// record) padded past 255 bytes and wraps it in a Type 2 Tag TLV the way a real tag stores it,
/// then runs the wrapper through find_mifare_ultralight_ndef_ to locate it, and feeds the located slice back
/// into NdefMessage to confirm all three records survive the round trip.
TEST(FindMifareUltralightNdef, LongFormMessageRoundTripsThroughNdefMessage) {
  nfc::NdefMessage message;
  message.add_uri_record("https://esphome.io/");
  message.add_text_record("Hello, ESPHome!");

  std::string json_payload = R"({"key":"value","pad":")" + std::string(255, 'x') + R"("})";
  auto json_record = make_unique<nfc::NdefRecord>();
  json_record->set_tnf(nfc::TNF_MIME_MEDIA);
  json_record->set_type("application/json");
  json_record->set_payload(json_payload);
  message.add_record(std::move(json_record));

  auto encoded = message.encode();
  ASSERT_GE(encoded.size(), 255u) << "test fixture must exercise the long-form TLV path";

  // Wrap exactly like write_mifare_ultralight_tag_ does: 0x03 FF LEN_HI LEN_LO ... 0xFE
  std::vector<uint8_t> wrapped = {0x03, 0xFF, static_cast<uint8_t>((encoded.size() >> 8) & 0xFF),
                                  static_cast<uint8_t>(encoded.size() & 0xFF)};
  wrapped.insert(wrapped.end(), encoded.begin(), encoded.end());
  wrapped.push_back(0xFE);

  // Real tag memory is [page 3 (4 bytes, unused here)] [wrapped TLV ...].
  std::vector<uint8_t> tag_memory = {0x00, 0x00, 0x00, 0x00};
  tag_memory.insert(tag_memory.end(), wrapped.begin(), wrapped.end());

  // find_mifare_ultralight_ndef_ only ever sees the first 16 bytes (pages 3-6) when locating the
  // TLV header -- exactly what read_mifare_ultralight_tag_'s first, fixed-size read provides.
  std::vector<uint8_t> page_3_to_6(tag_memory.begin(), tag_memory.begin() + 16);

  TestablePN532 pn532;
  uint32_t message_length = 0;
  uint8_t message_start_index = 0;
  ASSERT_TRUE(pn532.find_mifare_ultralight_ndef_(page_3_to_6, message_length, message_start_index));
  EXPECT_EQ(message_length, encoded.size());
  EXPECT_EQ(message_start_index, 4);

  // Reconstruct what read_mifare_ultralight_tag_ hands to NdefMessage: page 3 trimmed off, then
  // sliced to [message_start_index, message_start_index + message_length).
  std::vector<uint8_t> full_tag_data(tag_memory.begin() + 4, tag_memory.end());
  std::vector<uint8_t> message_bytes(full_tag_data.begin() + message_start_index,
                                     full_tag_data.begin() + message_start_index + message_length);

  nfc::NdefMessage parsed(message_bytes);
  const auto &records = parsed.get_records();
  ASSERT_EQ(records.size(), 3u);
  EXPECT_EQ(records[0]->get_type(), "U");
  EXPECT_EQ(records[0]->get_payload(), "https://esphome.io/");
  EXPECT_EQ(records[1]->get_type(), "T");
  EXPECT_EQ(records[1]->get_payload(), "Hello, ESPHome!");
  EXPECT_EQ(records[2]->get_type(), "application/json");
  EXPECT_EQ(records[2]->get_payload(), json_payload);
}

}  // namespace esphome::pn532::testing
