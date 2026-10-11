#include <gtest/gtest.h>

#include <deque>

#include "esphome/components/pn71xx/pn71xx.h"

namespace esphome::pn71xx {

namespace {

// Stands in for the bus: records every frame written and replays queued frames on read.
class FakePN71xx : public PN71xx {
 public:
  using PN71xx::card_emu_t4t_get_response_;
  using PN71xx::discovered_endpoint_;
  using PN71xx::erase_tag_;
  using PN71xx::find_or_add_tag_;
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
    const auto encoded = tx.encode();
    this->written.emplace_back(encoded.begin(), encoded.end());
    return nfc::STATUS_OK;
  }
};

std::vector<uint8_t> apdu(std::initializer_list<uint8_t> bytes) {
  std::vector<uint8_t> msg = {nfc::NCI_PKT_MT_DATA, 0x00, static_cast<uint8_t>(bytes.size())};
  msg.insert(msg.end(), bytes);
  return msg;
}

std::vector<uint8_t> respond(FakePN71xx &nfcc, std::initializer_list<uint8_t> bytes) {
  CardEmuResponse response;
  nfcc.card_emu_t4t_get_response_(apdu(bytes), response);
  return {response.begin(), response.end()};
}

void select_ndef_file(FakePN71xx &nfcc) {
  respond(nfcc, {0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00});
  respond(nfcc, {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x04});
}

std::vector<uint8_t> bytes_of(const nfc::NciMessage &msg) {
  return {msg.get_message().begin(), msg.get_message().end()};
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
  EXPECT_EQ(bytes_of(rx), (std::vector<uint8_t>{0x41, 0x06, 0x01, 0x00}));
  EXPECT_EQ(nfcc.written.size(), 1u);
}

TEST(PN71xxTransceive, NotificationAloneIsNotAResponse) {
  FakePN71xx nfcc;
  nfcc.to_read.push_back({0x61, 0x06, 0x02, 0x00, 0x00});
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DEACTIVATE_OID, {0x00});
  nfc::NciMessage rx;
  EXPECT_NE(nfcc.transceive_(tx, rx), nfc::STATUS_OK);
}

// A late response to an earlier, timed-out command must not be taken as the response to this one.
TEST(PN71xxTransceive, SkipsStaleResponseFromEarlierCommand) {
  FakePN71xx nfcc;
  nfcc.to_read.push_back({0x41, 0x06, 0x01, 0x00});  // RF_DEACTIVATE_RSP, arriving late
  nfcc.to_read.push_back({0x41, 0x03, 0x01, 0x00});  // RF_DISCOVER_RSP
  nfc::NciMessage tx(nfc::NCI_PKT_MT_CTRL_COMMAND, nfc::RF_GID, nfc::RF_DISCOVER_OID, {0x00});
  nfc::NciMessage rx;
  EXPECT_EQ(nfcc.transceive_(tx, rx), nfc::STATUS_OK);
  EXPECT_EQ(bytes_of(rx), (std::vector<uint8_t>{0x41, 0x03, 0x01, 0x00}));
  EXPECT_EQ(nfcc.written.size(), 1u);
}

nfc::NfcTagUid uid_of(uint8_t last) { return {0x04, 0x00, 0x00, last}; }

// Erasing an entry keeps the others in order and frees the tag of the slot that is dropped.
TEST(PN71xxTagCache, EraseKeepsOrderAndFreesTail) {
  FakePN71xx nfcc;
  for (uint8_t i = 0; i < 3; i++) {
    nfcc.find_or_add_tag_(nfc::PROT_T2T, uid_of(i));
  }
  nfcc.erase_tag_(1);
  ASSERT_EQ(nfcc.discovered_endpoint_.size(), 2u);
  EXPECT_EQ(nfcc.discovered_endpoint_[0].tag->get_uid()[3], 0);
  EXPECT_EQ(nfcc.discovered_endpoint_[1].tag->get_uid()[3], 2);
  EXPECT_EQ(nfcc.discovered_endpoint_.data()[2].tag, nullptr);
  nfcc.erase_tag_(1);
  ASSERT_EQ(nfcc.discovered_endpoint_.size(), 1u);
  EXPECT_EQ(nfcc.discovered_endpoint_.data()[1].tag, nullptr);
}

// A full cache evicts the entry seen longest ago instead of refusing the new tag.
TEST(PN71xxTagCache, FullCacheEvictsOldest) {
  FakePN71xx nfcc;
  for (uint8_t i = 0; i < MAX_DISCOVERED_ENDPOINTS; i++) {
    const size_t loc = nfcc.find_or_add_tag_(nfc::PROT_T2T, uid_of(i));
    nfcc.discovered_endpoint_[loc].last_seen = 100 + i;
  }
  nfcc.discovered_endpoint_[3].last_seen = 1;  // seen longest ago
  const size_t loc = nfcc.find_or_add_tag_(nfc::PROT_T2T, uid_of(0x99));
  ASSERT_EQ(nfcc.discovered_endpoint_.size(), MAX_DISCOVERED_ENDPOINTS);
  EXPECT_EQ(nfcc.discovered_endpoint_[loc].tag->get_uid()[3], 0x99);
  for (const auto &endpoint : nfcc.discovered_endpoint_) {
    EXPECT_NE(endpoint.tag->get_uid()[3], 3);
  }
  // a known UID is found, not added again
  EXPECT_EQ(nfcc.find_or_add_tag_(nfc::PROT_T2T, uid_of(0x99)), loc);
  EXPECT_EQ(nfcc.discovered_endpoint_.size(), MAX_DISCOVERED_ENDPOINTS);
}

// Bytes that do not fit the packet are dropped and the length byte stays consistent.
TEST(PN71xxNciMessage, AppendStopsAtPacketSize) {
  nfc::NciMessage msg(nfc::NCI_PKT_MT_DATA, {0x01});
  std::vector<uint8_t> big(300, 0xAA);
  msg.append(big);
  const auto encoded = msg.encode();
  EXPECT_EQ(encoded.size(), nfc::NCI_PKT_MAX_SIZE);
  EXPECT_EQ(msg.get_payload_size(), nfc::NCI_PKT_MAX_PAYLOAD_SIZE);
}

// A read that could not fit the status bytes into one packet is refused.
TEST(PN71xxCardEmulation, OversizedReadIsRejected) {
  FakePN71xx nfcc;
  nfcc.set_tag_emulation_message("https://www.home-assistant.io/tag/pulse_ce");
  select_ndef_file(nfcc);
  EXPECT_EQ(respond(nfcc, {0x00, 0xB0, 0x00, 0x00, 0xFE}), SW_NOT_FOUND);
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
