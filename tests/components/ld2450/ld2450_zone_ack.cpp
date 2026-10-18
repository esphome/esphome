#include <algorithm>
#include "common.h"

namespace esphome::ld2450::testing {

namespace {
// Successful zone query ACK (40 bytes), zone type 0x01, with the given coordinate bytes; missing ones are zero.
std::vector<uint8_t> make_zone_ack(std::vector<uint8_t> coords) {
  coords.resize(24, 0);
  std::vector<uint8_t> frame = {0xFD, 0xFC, 0xFB, 0xFA, 0x1E, 0x00, 0xC1, 0x01, 0x00, 0x00, 0x01, 0x00};
  frame.insert(frame.end(), coords.begin(), coords.end());
  frame.insert(frame.end(), {0x04, 0x03, 0x02, 0x01});
  return frame;
}

// Zone ACK with zone 0 at (100, 200) to (1500, 3000) and the other zones cleared.
std::vector<uint8_t> make_complete_zone_ack() {
  return make_zone_ack({0x64, 0x00, 0xC8, 0x00, 0xDC, 0x05, 0xB8, 0x0B});
}

// The first 16 bytes of a zone ACK whose zone 0 starts at (772, 258): its last four bytes look like a footer.
std::vector<uint8_t> make_truncated_zone_ack() {
  return {0xFD, 0xFC, 0xFB, 0xFA, 0x1E, 0x00, 0xC1, 0x01, 0x00, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
}

// Successful disable-configuration ACK: a four-byte body.
std::vector<uint8_t> make_disable_conf_ack() {
  auto frame = make_ack_frame();
  frame[6] = 0xFE;
  return frame;
}

void expect_zone(const TestableLD2450 &ld2450, size_t index, int x1, int y1, int x2, int y2) {
  EXPECT_EQ(ld2450.zone_config_[index].x1, x1) << "zone " << index;
  EXPECT_EQ(ld2450.zone_config_[index].y1, y1) << "zone " << index;
  EXPECT_EQ(ld2450.zone_config_[index].x2, x2) << "zone " << index;
  EXPECT_EQ(ld2450.zone_config_[index].y2, y2) << "zone " << index;
}
}  // namespace

class LD2450ZoneAckTest : public ::testing::Test {
 protected:
  void SetUp() override { this->ld2450_.set_uart_parent(&this->mock_uart_); }
  void set_all_zones_(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    for (auto &zone : this->ld2450_.zone_config_) {
      zone = {x1, y1, x2, y2};
    }
  }
  void expect_all_zones_(int x1, int y1, int x2, int y2) {
    for (size_t i = 0; i < MAX_ZONES; i++) {
      expect_zone(this->ld2450_, i, x1, y1, x2, y2);
    }
  }

  MockUARTComponent mock_uart_;
  TestableLD2450 ld2450_;
};

TEST_F(LD2450ZoneAckTest, CmdFooterBytesInZonePayloadDoNotTruncateAck) {
  // x1=772 and y1=258 are the command footer bytes, followed by x2=1500 and y2=3000.
  static constexpr uint8_t COORDINATES[] = {0x04, 0x03, 0x02, 0x01, 0xDC, 0x05, 0xB8, 0x0B};
  for (uint8_t zone = 0; zone < MAX_ZONES; zone++) {
    std::vector<uint8_t> coords(24, 0);
    std::memcpy(coords.data() + zone * 8, COORDINATES, sizeof(COORDINATES));
    const auto frame = make_zone_ack(coords);
    std::memset(this->ld2450_.buffer_data_, 0xEE, MAX_LINE_LENGTH);

    const size_t split = 12 + zone * 8 + 4;
    this->ld2450_.feed(std::vector<uint8_t>(frame.begin(), frame.begin() + split));
    EXPECT_EQ(this->ld2450_.buffer_pos_, split) << "zone " << static_cast<int>(zone);
    this->ld2450_.feed(std::vector<uint8_t>(frame.begin() + split, frame.end()));
    EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
      if (i == zone) {
        expect_zone(this->ld2450_, i, 772, 258, 1500, 3000);
      } else {
        expect_zone(this->ld2450_, i, 0, 0, 0, 0);
      }
    }
  }
}

TEST_F(LD2450ZoneAckTest, CompleteZoneAckWithoutEmbeddedFooter) {
  this->set_all_zones_(100, 200, 300, 400);
  this->ld2450_.feed(make_zone_ack({}));
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  this->expect_all_zones_(0, 0, 0, 0);
}

TEST_F(LD2450ZoneAckTest, FailedZoneQueryAckDoesNotWaitForCoordinates) {
  this->set_all_zones_(100, 200, 300, 400);
  auto frame = make_ack_frame();
  frame[6] = 0xC1;
  frame[8] = 0x01;
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  this->expect_all_zones_(100, 200, 300, 400);
}

TEST_F(LD2450ZoneAckTest, ValidZoneAckWithEmbeddedFooterAndHeaderIsNotResynced) {
  const auto frame = make_zone_ack({0x04, 0x03, 0x02, 0x01, 0xFD, 0xFC, 0xFB, 0xFA,    // 772, 258, -771, -1285
                                    0x1E, 0x00, 0xC1, 0x01, 0x00, 0x00, 0x01, 0x00,    // 30, 449, 0, 1
                                    0x64, 0x00, 0xC8, 0x00, 0xDC, 0x05, 0xB8, 0x0B});  // 100, 200, 1500, 3000
  // The first 36 bytes equal a truncated ACK followed by a complete one; only the footer at offset 36 differs.
  auto stream = make_truncated_zone_ack();
  const auto complete = make_complete_zone_ack();
  stream.insert(stream.end(), complete.begin(), complete.end());
  ASSERT_TRUE(std::equal(frame.begin(), frame.begin() + 36, stream.begin()));

  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  expect_zone(this->ld2450_, 0, 772, 258, -771, -1285);
  expect_zone(this->ld2450_, 1, 30, 449, 0, 1);
  expect_zone(this->ld2450_, 2, 100, 200, 1500, 3000);
}

TEST_F(LD2450ZoneAckTest, TruncatedZoneAckThenCompleteZoneAckRecovers) {
  this->ld2450_.feed(make_truncated_zone_ack());
  this->ld2450_.feed(make_complete_zone_ack());
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  expect_zone(this->ld2450_, 0, 100, 200, 1500, 3000);
}

TEST_F(LD2450ZoneAckTest, TruncatedZoneAckThenPeriodicFrameIsParsed) {
  this->ld2450_.zone_config_[0] = {100, 200, 300, 400};
  int frames = 0;
  this->ld2450_.add_on_data_callback([&frames]() { frames++; });

  this->ld2450_.feed(make_truncated_zone_ack());
  this->ld2450_.feed(make_periodic_frame());

  // The truncated ACK is not decoded; the periodic frame is recovered through the data header.
  EXPECT_EQ(frames, 1);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  expect_zone(this->ld2450_, 0, 100, 200, 300, 400);
}

TEST_F(LD2450ZoneAckTest, TruncatedZoneAckThenShortAckThenCompleteZoneAckRecovers) {
  this->ld2450_.feed(make_truncated_zone_ack());
  this->ld2450_.feed(make_disable_conf_ack());
  this->ld2450_.feed(make_complete_zone_ack());
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  expect_zone(this->ld2450_, 0, 100, 200, 1500, 3000);
}

TEST_F(LD2450ZoneAckTest, ShortAckInRetainedSuffixIsDispatchedOnResync) {
  const auto complete = make_complete_zone_ack();
  this->ld2450_.feed(make_truncated_zone_ack());
  this->ld2450_.feed(make_disable_conf_ack());
  // Ten bytes of the next zone ACK bring the buffer to 40 bytes without a footer.
  this->ld2450_.feed(std::vector<uint8_t>(complete.begin(), complete.begin() + 10));
  // The short ACK was replayed and consumed; only the new zone ACK prefix remains.
  EXPECT_EQ(this->ld2450_.buffer_pos_, 10);
  EXPECT_EQ(this->ld2450_.buffer_data_[6], 0xC1);
  this->ld2450_.feed(std::vector<uint8_t>(complete.begin() + 10, complete.end()));
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  expect_zone(this->ld2450_, 0, 100, 200, 1500, 3000);
}

TEST_F(LD2450ZoneAckTest, MultipleCompleteFramesInRetainedSuffixAreDrained) {
  // Truncated zone ACK without any footer, followed by two complete short ACKs (11 + 14 + 14 = 39).
  const auto truncated = make_truncated_zone_ack();
  std::vector<uint8_t> stream(truncated.begin(), truncated.begin() + 11);
  const auto disable_ack = make_disable_conf_ack();
  stream.insert(stream.end(), disable_ack.begin(), disable_ack.end());
  const auto enable_ack = make_ack_frame();
  stream.insert(stream.end(), enable_ack.begin(), enable_ack.end());
  this->ld2450_.feed(stream);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 39);  // the C1 header still claims the buffer
  // The 40th byte triggers the resync; both buffered ACKs are drained, leaving only this byte.
  const auto complete = make_complete_zone_ack();
  this->ld2450_.feed({complete[0]});
  EXPECT_EQ(this->ld2450_.buffer_pos_, 1);
  EXPECT_EQ(this->ld2450_.buffer_data_[0], 0xFD);
  this->ld2450_.feed(std::vector<uint8_t>(complete.begin() + 1, complete.end()));
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  expect_zone(this->ld2450_, 0, 100, 200, 1500, 3000);
}

}  // namespace esphome::ld2450::testing
