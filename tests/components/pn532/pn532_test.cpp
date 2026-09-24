#include <gtest/gtest.h>

#include <deque>

#include "esphome/components/pn532/pn532.h"

namespace esphome::pn532 {

namespace {

// Stands in for the bus: acknowledges every command and answers with queued response payloads.
class FakePN532 : public PN532 {
 public:
  using PN532::auth_mifare_classic_block_;
  using PN532::read_mifare_ultralight_bytes_;
  using PN532::read_mifare_classic_block_;
  using PN532::write_mifare_classic_block_;
  using PN532::write_mifare_ultralight_page_;

  std::deque<std::vector<uint8_t>> responses;
  std::vector<std::vector<uint8_t>> written;

 protected:
  bool is_read_ready() override { return true; }
  bool write_data(const std::vector<uint8_t> &data) override {
    this->written.push_back(data);
    return true;
  }
  // only used for ACK frames; index 0 is the I2C status byte
  bool read_data(std::vector<uint8_t> &data, uint8_t len) override {
    data = {0x01, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    return true;
  }
  bool read_response(uint8_t command, std::vector<uint8_t> &data) override {
    if (this->responses.empty())
      return false;
    data = this->responses.front();
    this->responses.pop_front();
    return true;
  }
};

// Extracts the command bytes (after TFI) from a normal information frame
std::vector<uint8_t> frame_data(const std::vector<uint8_t> &frame) {
  // preamble, start code (2), LEN, LCS, TFI, data..., DCS, postamble
  return std::vector<uint8_t>(frame.begin() + 6, frame.end() - 2);
}

}  // namespace

TEST(PN532TagType, FromSelRes) {
  EXPECT_EQ(tag_type_from_sel_res(0x08), nfc::TAG_TYPE_MIFARE_CLASSIC);  // Classic 1K
  EXPECT_EQ(tag_type_from_sel_res(0x18), nfc::TAG_TYPE_MIFARE_CLASSIC);  // Classic 4K
  EXPECT_EQ(tag_type_from_sel_res(0x09), nfc::TAG_TYPE_MIFARE_CLASSIC);  // Mini
  EXPECT_EQ(tag_type_from_sel_res(0x01), nfc::TAG_TYPE_MIFARE_CLASSIC);  // TNP3xxx
  EXPECT_EQ(tag_type_from_sel_res(0x00), nfc::TAG_TYPE_2);               // Ultralight / NTAG
  EXPECT_EQ(tag_type_from_sel_res(0x20), nfc::TAG_TYPE_4);               // ISO-DEP (phones, DESFire)
  EXPECT_EQ(tag_type_from_sel_res(0x40), nfc::TAG_TYPE_UNKNOWN);
}

// A failed write (status byte other than 0x00) must be reported as a failure.
TEST(PN532Mifare, ClassicWriteChecksStatus) {
  FakePN532 pn532;
  const uint8_t block[16] = {};
  pn532.responses.push_back({0x14});  // authentication error
  EXPECT_FALSE(pn532.write_mifare_classic_block_(4, block, sizeof(block)));
  pn532.responses.push_back({0x00});
  EXPECT_TRUE(pn532.write_mifare_classic_block_(4, block, sizeof(block)));
}

TEST(PN532Mifare, UltralightWriteChecksStatus) {
  FakePN532 pn532;
  const uint8_t page[4] = {};
  pn532.responses.push_back({0x01});  // timeout
  EXPECT_FALSE(pn532.write_mifare_ultralight_page_(4, page, sizeof(page)));
  pn532.responses.push_back({0x00});
  EXPECT_TRUE(pn532.write_mifare_ultralight_page_(4, page, sizeof(page)));
}

TEST(PN532Mifare, ClassicReadRejectsBadResponses) {
  FakePN532 pn532;
  std::vector<uint8_t> data;
  pn532.responses.emplace_back();  // empty response
  EXPECT_FALSE(pn532.read_mifare_classic_block_(4, data));
  data.clear();
  pn532.responses.push_back({0x00, 0x01, 0x02});  // short block
  EXPECT_FALSE(pn532.read_mifare_classic_block_(4, data));

  std::vector<uint8_t> good(17, 0xAB);
  good[0] = 0x00;
  pn532.responses.push_back(good);
  data.clear();
  EXPECT_TRUE(pn532.read_mifare_classic_block_(4, data));
  EXPECT_EQ(data, std::vector<uint8_t>(16, 0xAB));
}

// Authentication carries exactly 4 UID bytes: the last 4 of a 7-byte UID.
TEST(PN532Mifare, AuthSendsFourUidBytes) {
  FakePN532 pn532;
  nfc::NfcTagUid uid = {0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
  pn532.responses.push_back({0x00});
  EXPECT_TRUE(pn532.auth_mifare_classic_block_(uid, 4, nfc::MIFARE_CMD_AUTH_A, nfc::NDEF_KEY));
  ASSERT_EQ(pn532.written.size(), 1u);
  const auto cmd = frame_data(pn532.written[0]);
  // InDataExchange, Tg, Cmd, Addr, key (6), UID (4)
  ASSERT_EQ(cmd.size(), 14u);
  EXPECT_EQ(std::vector<uint8_t>(cmd.end() - 4, cmd.end()), (std::vector<uint8_t>{0x33, 0x44, 0x55, 0x66}));
}

// Reads in 16-byte chunks, keeps only the bytes asked for, and advances 4 pages per READ.
TEST(PN532Mifare, UltralightReadTrimsLastChunk) {
  FakePN532 pn532;
  std::vector<uint8_t> first(17), second(17);
  first[0] = second[0] = 0x00;  // status
  for (uint8_t i = 0; i < 16; i++) {
    first[i + 1] = i;
    second[i + 1] = 0x10 + i;
  }
  pn532.responses.push_back(first);
  pn532.responses.push_back(second);

  std::vector<uint8_t> data;
  ASSERT_TRUE(pn532.read_mifare_ultralight_bytes_(4, 20, data));
  ASSERT_EQ(data.size(), 20u);
  EXPECT_EQ(data[15], 15);
  EXPECT_EQ(data[16], 0x10);
  EXPECT_EQ(data[19], 0x13);

  ASSERT_EQ(pn532.written.size(), 2u);
  EXPECT_EQ(frame_data(pn532.written[0]).back(), 4);  // READ page 4
  EXPECT_EQ(frame_data(pn532.written[1]).back(), 8);  // then page 8
}

TEST(PN532Mifare, UltralightReadRejectsBadResponses) {
  FakePN532 pn532;
  std::vector<uint8_t> data;
  pn532.responses.push_back({0x00, 0x01, 0x02});  // short response
  EXPECT_FALSE(pn532.read_mifare_ultralight_bytes_(4, 16, data));

  std::vector<uint8_t> failed(17, 0x00);
  failed[0] = 0x01;  // timeout status
  pn532.responses.push_back(failed);
  data.clear();
  EXPECT_FALSE(pn532.read_mifare_ultralight_bytes_(4, 16, data));
}

}  // namespace esphome::pn532
