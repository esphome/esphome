#include <gtest/gtest.h>

#include <deque>

#include "esphome/components/pn71xx/pn71xx.h"

namespace esphome::pn71xx {

namespace {

// Stands in for the bus: records every frame written and replays queued frames on read.
class FakePN71xx : public PN71xx {
 public:
  using PN71xx::card_emu_t4t_get_response_;
  using PN71xx::transceive_;

  std::deque<std::vector<uint8_t>> to_read;
  std::vector<std::vector<uint8_t>> written;
  uint8_t write_failures{0};

 protected:
  uint8_t verify_reset(nfc::NciMessage &rx, bool reset_config) override { return nfc::STATUS_OK; }
  uint8_t process_init_response(nfc::NciMessage &rx) override { return nfc::STATUS_OK; }
  std::span<const uint8_t> pmu_config() const override { return {}; }
  std::span<const uint8_t> listen_mode_routing_config() const override { return {}; }

  uint8_t read_nfcc(nfc::NciMessage &rx, uint16_t timeout) override {
    if (this->to_read.empty())
      return nfc::STATUS_FAILED;
    rx = nfc::NciMessage(this->to_read.front());
    this->to_read.pop_front();
    return nfc::STATUS_OK;
  }
  uint8_t write_nfcc(nfc::NciMessage &tx) override {
    if (this->write_failures > 0) {
      this->write_failures--;
      return nfc::STATUS_FAILED;
    }
    this->written.push_back(tx.encode());
    return nfc::STATUS_OK;
  }
};

std::vector<uint8_t> apdu(std::initializer_list<uint8_t> bytes) {
  std::vector<uint8_t> msg = {nfc::NCI_PKT_MT_DATA, 0x00, static_cast<uint8_t>(bytes.size())};
  msg.insert(msg.end(), bytes);
  return msg;
}

std::vector<uint8_t> respond(FakePN71xx &nfcc, std::initializer_list<uint8_t> bytes) {
  std::vector<uint8_t> response;
  nfcc.card_emu_t4t_get_response_(apdu(bytes), response);
  return response;
}

void select_ndef_file(FakePN71xx &nfcc) {
  respond(nfcc, {0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00});
  respond(nfcc, {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x04});
}

const std::vector<uint8_t> SW_OK = {0x90, 0x00};
const std::vector<uint8_t> SW_NOT_FOUND = {0x6A, 0x82};

}  // namespace

// A timed-out read must not cause the command to be sent again (NCI forbids a second command before the response).
TEST(PN71xxTransceive, ReadTimeoutDoesNotResend) {
  FakePN71xx nfcc;
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DEACTIVATE_OID, {0x00});
  nfc::NciMessage rx;
  EXPECT_NE(nfcc.transceive_(tx, rx), nfc::STATUS_OK);
  EXPECT_EQ(nfcc.written.size(), 1u);
}

// A notification with the same GID/OID as the response (RF_DEACTIVATE_NTF) is not mistaken for it.
TEST(PN71xxTransceive, SkipsNotificationAheadOfResponse) {
  FakePN71xx nfcc;
  nfcc.to_read.push_back({0x61, 0x06, 0x02, 0x00, 0x00});
  nfcc.to_read.push_back({0x41, 0x06, 0x01, 0x00});
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DEACTIVATE_OID, {0x00});
  nfc::NciMessage rx;
  EXPECT_EQ(nfcc.transceive_(tx, rx), nfc::STATUS_OK);
  EXPECT_EQ(rx.get_message(), (std::vector<uint8_t>{0x41, 0x06, 0x01, 0x00}));
  EXPECT_EQ(nfcc.written.size(), 1u);
}

TEST(PN71xxTransceive, NotificationAloneIsNotAResponse) {
  FakePN71xx nfcc;
  nfcc.to_read.push_back({0x61, 0x06, 0x02, 0x00, 0x00});
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DEACTIVATE_OID, {0x00});
  nfc::NciMessage rx;
  EXPECT_NE(nfcc.transceive_(tx, rx), nfc::STATUS_OK);
}

// A refused write (e.g. NFCC in standby) is sent again.
TEST(PN71xxTransceive, RefusedWriteIsRetried) {
  FakePN71xx nfcc;
  nfcc.write_failures = 1;
  nfcc.to_read.push_back({0x41, 0x06, 0x01, 0x00});
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DEACTIVATE_OID, {0x00});
  nfc::NciMessage rx;
  EXPECT_EQ(nfcc.transceive_(tx, rx), nfc::STATUS_OK);
  EXPECT_EQ(nfcc.written.size(), 1u);
}

TEST(PN71xxCardEmulation, CcReadOutOfRangeIsRejected) {
  FakePN71xx nfcc;
  nfcc.set_tag_emulation_message("https://www.home-assistant.io/tag/test", false);
  respond(nfcc, {0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00});
  respond(nfcc, {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x03});
  // the CC file is 15 bytes; reading 17 or reading far past its end must not return memory beyond it
  EXPECT_EQ(respond(nfcc, {0x00, 0xB0, 0x00, 0x00, 0x11}), SW_NOT_FOUND);
  respond(nfcc, {0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00});
  respond(nfcc, {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x03});
  EXPECT_EQ(respond(nfcc, {0x00, 0xB0, 0x01, 0x00, 0x0F}), SW_NOT_FOUND);
}

TEST(PN71xxCardEmulation, CcReadInRange) {
  FakePN71xx nfcc;
  nfcc.set_tag_emulation_message("https://www.home-assistant.io/tag/test", false);
  respond(nfcc, {0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00});
  respond(nfcc, {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x03});
  auto response = respond(nfcc, {0x00, 0xB0, 0x00, 0x00, 0x0F});
  ASSERT_EQ(response.size(), sizeof(CARD_EMU_T4T_CC) + 2);
  EXPECT_TRUE(std::equal(std::begin(CARD_EMU_T4T_CC), std::end(CARD_EMU_T4T_CC), response.begin()));
}

// Reading the NDEF file in small chunks returns NLEN followed by the message, in order.
TEST(PN71xxCardEmulation, ChunkedNdefReadMatchesFile) {
  FakePN71xx nfcc;
  auto message = std::make_shared<nfc::NdefMessage>();
  message->add_uri_record("https://www.home-assistant.io/tag/0123456789abcdef");
  const auto encoded = message->encode();
  nfcc.set_tag_emulation_message(message);
  select_ndef_file(nfcc);

  std::vector<uint8_t> expected = {static_cast<uint8_t>(encoded.size() >> 8),
                                   static_cast<uint8_t>(encoded.size() & 0xFF)};
  expected.insert(expected.end(), encoded.begin(), encoded.end());

  std::vector<uint8_t> file;
  for (size_t offset = 0; offset < expected.size(); offset += 5) {
    const uint8_t length = std::min<size_t>(5, expected.size() - offset);
    auto response =
        respond(nfcc, {0x00, 0xB0, static_cast<uint8_t>(offset >> 8), static_cast<uint8_t>(offset), length});
    ASSERT_EQ(response.size(), length + 2u);
    EXPECT_EQ(std::vector<uint8_t>(response.end() - 2, response.end()), SW_OK);
    file.insert(file.end(), response.begin(), response.end() - 2);
  }
  EXPECT_EQ(file, expected);
}

TEST(PN71xxCardEmulation, NdefReadPastEndIsRejected) {
  FakePN71xx nfcc;
  nfcc.set_tag_emulation_message("https://www.home-assistant.io/tag/test", false);
  select_ndef_file(nfcc);
  EXPECT_EQ(respond(nfcc, {0x00, 0xB0, 0x00, 0x02, 0xFD}), SW_NOT_FOUND);
}

TEST(PN71xxCardEmulation, TruncatedApdusAreRejected) {
  FakePN71xx nfcc;
  nfcc.set_tag_emulation_message("https://www.home-assistant.io/tag/test", false);
  select_ndef_file(nfcc);
  EXPECT_EQ(respond(nfcc, {0x00, 0xB0, 0x00}), SW_NOT_FOUND);
  select_ndef_file(nfcc);
  // UPDATE BINARY claiming 16 bytes of data but carrying only 2
  EXPECT_EQ(respond(nfcc, {0x00, 0xD6, 0x00, 0x00, 0x10, 0x00, 0x00}), SW_NOT_FOUND);
}

// A message too large for the emulated NDEF file is refused when it is set, keeping the previous one.
TEST(PN71xxCardEmulation, OversizedMessageRejectedWhenSet) {
  FakePN71xx nfcc;
  nfcc.set_tag_emulation_message("https://www.home-assistant.io/tag/test", false);
  nfcc.set_tag_emulation_message("https://www.home-assistant.io/tag/" + std::string(300, 'x'), false);
  select_ndef_file(nfcc);
  auto response = respond(nfcc, {0x00, 0xB0, 0x00, 0x00, 0x02});
  ASSERT_EQ(response.size(), 4u);
  EXPECT_LT((response[0] << 8) | response[1], 0xFF - 2);
  EXPECT_EQ(std::vector<uint8_t>(response.end() - 2, response.end()), SW_OK);
}

}  // namespace esphome::pn71xx
