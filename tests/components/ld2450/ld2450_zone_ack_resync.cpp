#include <algorithm>
#include "common.h"

namespace esphome::ld2450::testing {

// Successful zone query ACK (40 bytes), zone type 0x01, with the given 24 coordinate bytes.
static std::vector<uint8_t> make_zone_ack(const std::vector<uint8_t> &coords) {
  std::vector<uint8_t> frame = {0xFD, 0xFC, 0xFB, 0xFA, 0x1E, 0x00, 0xC1, 0x01, 0x00, 0x00, 0x01, 0x00};
  frame.insert(frame.end(), coords.begin(), coords.end());
  frame.insert(frame.end(), {0x04, 0x03, 0x02, 0x01});
  return frame;
}

// Truncated zone ACK from the independent regression: its last four bytes look like a footer.
static std::vector<uint8_t> make_truncated_zone_ack() {
  return {0xFD, 0xFC, 0xFB, 0xFA, 0x1E, 0x00, 0xC1, 0x01, 0x00, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
}

class LD2450ZoneAckResyncTest : public ::testing::Test {
 protected:
  void SetUp() override { this->ld2450_.set_uart_parent(&this->mock_uart_); }

  MockUARTComponent mock_uart_;
  TestableLD2450 ld2450_;
};

TEST_F(LD2450ZoneAckResyncTest, ValidZoneAckWithEmbeddedFooterAndHeaderIsNotResynced) {
  auto frame = make_zone_ack({0x04, 0x03, 0x02, 0x01, 0xFD, 0xFC, 0xFB, 0xFA,    // 772, 258, -771, -1285
                              0x1E, 0x00, 0xC1, 0x01, 0x00, 0x00, 0x01, 0x00,    // 30, 449, 0, 1
                              0x64, 0x00, 0xC8, 0x00, 0xDC, 0x05, 0xB8, 0x0B});  // 100, 200, 1500, 3000
  // Identical to truncated + complete stream for 36 bytes; only the footer at offset 36 differs.
  auto stream = make_truncated_zone_ack();
  auto complete =
      make_zone_ack({0x64, 0x00, 0xC8, 0x00, 0xDC, 0x05, 0xB8, 0x0B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  stream.insert(stream.end(), complete.begin(), complete.end());
  ASSERT_TRUE(std::equal(frame.begin(), frame.begin() + 36, stream.begin()));

  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  EXPECT_EQ(this->ld2450_.zone_config_[0].x1, 772);
  EXPECT_EQ(this->ld2450_.zone_config_[0].y1, 258);
  EXPECT_EQ(this->ld2450_.zone_config_[0].x2, -771);
  EXPECT_EQ(this->ld2450_.zone_config_[0].y2, -1285);
  EXPECT_EQ(this->ld2450_.zone_config_[1].x1, 30);
  EXPECT_EQ(this->ld2450_.zone_config_[1].y1, 449);
  EXPECT_EQ(this->ld2450_.zone_config_[1].x2, 0);
  EXPECT_EQ(this->ld2450_.zone_config_[1].y2, 1);
  EXPECT_EQ(this->ld2450_.zone_config_[2].x1, 100);
  EXPECT_EQ(this->ld2450_.zone_config_[2].y1, 200);
  EXPECT_EQ(this->ld2450_.zone_config_[2].x2, 1500);
  EXPECT_EQ(this->ld2450_.zone_config_[2].y2, 3000);
}

TEST_F(LD2450ZoneAckResyncTest, TruncatedZoneAckThenPeriodicFrameIsParsed) {
  this->ld2450_.zone_config_[0] = {100, 200, 300, 400};
  int frames = 0;
  this->ld2450_.add_on_data_callback([&frames]() { frames++; });

  this->ld2450_.feed(make_truncated_zone_ack());
  this->ld2450_.feed(make_periodic_frame());

  // Truncated ACK is not decoded; the periodic frame is recovered via DATA header resync.
  EXPECT_EQ(frames, 1);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  EXPECT_EQ(this->ld2450_.zone_config_[0].x1, 100);
  EXPECT_EQ(this->ld2450_.zone_config_[0].y1, 200);
  EXPECT_EQ(this->ld2450_.zone_config_[0].x2, 300);
  EXPECT_EQ(this->ld2450_.zone_config_[0].y2, 400);
}

TEST_F(LD2450ZoneAckResyncTest, TruncatedZoneAckThenShortAckThenCompleteZoneAckRecovers) {
  this->ld2450_.feed(make_truncated_zone_ack());
  auto short_ack = make_ack_frame();
  short_ack[6] = 0xFE;  // Successful disable-configuration ACK has a four-byte body.
  this->ld2450_.feed(short_ack);
  auto complete =
      make_zone_ack({0x64, 0x00, 0xC8, 0x00, 0xDC, 0x05, 0xB8, 0x0B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  this->ld2450_.feed(complete);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  EXPECT_EQ(this->ld2450_.zone_config_[0].x1, 100);
  EXPECT_EQ(this->ld2450_.zone_config_[0].y1, 200);
  EXPECT_EQ(this->ld2450_.zone_config_[0].x2, 1500);
  EXPECT_EQ(this->ld2450_.zone_config_[0].y2, 3000);
}

namespace {
const std::vector<uint8_t> TRUNCATED_ZONE_ACK = {0xFD, 0xFC, 0xFB, 0xFA, 0x1E, 0x00, 0xC1, 0x01,
                                                 0x00, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
const std::vector<uint8_t> DISABLE_CONF_ACK = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFE,
                                               0x01, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01};
const std::vector<uint8_t> COMPLETE_ZONE_ACK = {0xFD, 0xFC, 0xFB, 0xFA, 0x1E, 0x00, 0xC1, 0x01, 0x00, 0x00,
                                                0x01, 0x00, 0x64, 0x00, 0xC8, 0x00, 0xDC, 0x05, 0xB8, 0x0B,
                                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x03, 0x02, 0x01};

void expect_zone0(const TestableLD2450 &ld2450) {
  EXPECT_EQ(ld2450.zone_config_[0].x1, 100);
  EXPECT_EQ(ld2450.zone_config_[0].y1, 200);
  EXPECT_EQ(ld2450.zone_config_[0].x2, 1500);
  EXPECT_EQ(ld2450.zone_config_[0].y2, 3000);
}
}  // namespace

TEST_F(LD2450ZoneAckResyncTest, ShortAckInRetainedSuffixIsDispatchedOnResync) {
  this->ld2450_.feed(TRUNCATED_ZONE_ACK);
  this->ld2450_.feed(DISABLE_CONF_ACK);
  // Ten bytes of the next zone ACK bring the buffer to 40 bytes without a footer.
  this->ld2450_.feed(std::vector<uint8_t>(COMPLETE_ZONE_ACK.begin(), COMPLETE_ZONE_ACK.begin() + 10));
  // The short ACK was replayed and consumed; only the new zone ACK prefix remains.
  EXPECT_EQ(this->ld2450_.buffer_pos_, 10);
  EXPECT_EQ(this->ld2450_.buffer_data_[6], 0xC1);
  this->ld2450_.feed(std::vector<uint8_t>(COMPLETE_ZONE_ACK.begin() + 10, COMPLETE_ZONE_ACK.end()));
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  expect_zone0(this->ld2450_);
}

TEST_F(LD2450ZoneAckResyncTest, MultipleCompleteFramesInRetainedSuffixAreDrained) {
  // Truncated zone ACK without any footer, followed by two complete short ACKs (11 + 14 + 14 = 39).
  std::vector<uint8_t> stream(TRUNCATED_ZONE_ACK.begin(), TRUNCATED_ZONE_ACK.begin() + 11);
  stream.insert(stream.end(), DISABLE_CONF_ACK.begin(), DISABLE_CONF_ACK.end());
  auto enable_ack = make_ack_frame();
  stream.insert(stream.end(), enable_ack.begin(), enable_ack.end());
  this->ld2450_.feed(stream);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 39);  // C1 header still claims the buffer
  // The 40th byte triggers resync; both buffered ACKs are drained, leaving only this byte.
  this->ld2450_.feed({COMPLETE_ZONE_ACK[0]});
  EXPECT_EQ(this->ld2450_.buffer_pos_, 1);
  EXPECT_EQ(this->ld2450_.buffer_data_[0], 0xFD);
  this->ld2450_.feed(std::vector<uint8_t>(COMPLETE_ZONE_ACK.begin() + 1, COMPLETE_ZONE_ACK.end()));
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  expect_zone0(this->ld2450_);
}

}  // namespace esphome::ld2450::testing
