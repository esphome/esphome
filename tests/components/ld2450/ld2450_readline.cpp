#include "common.h"
#include <cstring>
#include <vector>

namespace esphome::ld2450::testing {

class LD2450ReadlineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->ld2450_.set_uart_parent(&this->mock_uart_);
    // Ensure clean state
    ASSERT_EQ(this->ld2450_.buffer_pos_, 0);
  }

  MockUARTComponent mock_uart_;
  TestableLD2450 ld2450_;
};

// --- Good data tests ---

TEST_F(LD2450ReadlineTest, ValidPeriodicFrame) {
  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  // After a complete valid frame, buffer should be reset
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, ValidCommandAckFrame) {
  auto frame = make_ack_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, BackToBackPeriodicFrames) {
  auto frame = make_periodic_frame();
  for (int i = 0; i < 5; i++) {
    this->ld2450_.feed(frame);
    EXPECT_EQ(this->ld2450_.buffer_pos_, 0) << "Frame " << i << " not processed";
  }
}

TEST_F(LD2450ReadlineTest, BackToBackMixedFrames) {
  auto periodic = make_periodic_frame();
  auto ack = make_ack_frame();
  this->ld2450_.feed(periodic);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  this->ld2450_.feed(ack);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  this->ld2450_.feed(periodic);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

// --- Garbage then valid frame tests ---

TEST_F(LD2450ReadlineTest, GarbageThenValidFrame) {
  // Garbage bytes accumulate in the buffer but don't match any footer.
  // A valid frame follows; its footer resets the buffer and resyncs.
  std::vector<uint8_t> garbage = {0x01, 0x02, 0x03, 0x42, 0x99};
  this->ld2450_.feed(garbage);
  EXPECT_GT(this->ld2450_.buffer_pos_, 0);  // Garbage accumulated

  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  // Footer from the valid frame resyncs the parser
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

// --- Footer-based resynchronization tests ---

TEST_F(LD2450ReadlineTest, FooterInGarbageResyncs) {
  // Garbage containing a periodic frame footer (0x55 0xCC) triggers
  // a buffer reset, allowing the next frame to be parsed cleanly.
  std::vector<uint8_t> garbage_with_footer = {0x01, 0x02, 0x03, 0x04, 0x55, 0xCC};
  this->ld2450_.feed(garbage_with_footer);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);  // Footer reset the buffer

  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, CmdFooterInGarbageResyncs) {
  // Garbage containing a command frame footer (04 03 02 01) also resyncs.
  std::vector<uint8_t> garbage_with_footer = {0x10, 0x20, 0x30, 0x40, 0x04, 0x03, 0x02, 0x01};
  this->ld2450_.feed(garbage_with_footer);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);

  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

// --- Overflow recovery tests ---

TEST_F(LD2450ReadlineTest, OverflowResetsBuffer) {
  // Fill the buffer to capacity with filler that won't match any footer.
  // MAX_LINE_LENGTH is 45, usable is 44. The 45th byte triggers overflow.
  std::vector<uint8_t> overflow_data(MAX_LINE_LENGTH, 0x11);
  this->ld2450_.feed(overflow_data);
  // After overflow, buffer_pos_ resets to 0 (via the < 4 early return path)
  EXPECT_LT(this->ld2450_.buffer_pos_, 4);
}

TEST_F(LD2450ReadlineTest, OverflowThenValidFrame) {
  // Overflow, then a valid frame should be processed.
  std::vector<uint8_t> overflow_data(MAX_LINE_LENGTH, 0x11);
  this->ld2450_.feed(overflow_data);

  auto frame = make_periodic_frame();
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, BufferLargeEnoughForDesyncedFooter) {
  // The key fix: the buffer (45) is large enough that a desynced periodic frame's
  // footer (at most 30 bytes into the stream) will land inside the buffer before overflow.
  // Simulate starting 10 bytes into a periodic frame, then a full frame follows.
  std::vector<uint8_t> mid_frame = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
  // Then a complete periodic frame whose footer will land at position 40 (10 + 30),
  // well within the buffer size of 45.
  auto frame = make_periodic_frame();
  mid_frame.insert(mid_frame.end(), frame.begin(), frame.end());

  this->ld2450_.feed(mid_frame);
  // The footer from the frame should have triggered a reset
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

TEST_F(LD2450ReadlineTest, SimulatedRestartThenFrames) {
  // Simulate LD2450 restart: burst of garbage followed by valid periodic frames.
  // The garbage + first frame should fit in the buffer so the footer resyncs.
  std::vector<uint8_t> restart_noise = {
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // 8 bytes of mid-frame data
  };
  auto frame = make_periodic_frame();
  // 8 garbage + 30 frame = 38 bytes, well within buffer of 45
  restart_noise.insert(restart_noise.end(), frame.begin(), frame.end());

  this->ld2450_.feed(restart_noise);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);

  // Subsequent frames should work normally
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
}

class ZoneTestableLD2450 : public TestableLD2450 {
 public:
  using LD2450Component::zone_config_;
};

class LD2450ZoneAckTest : public ::testing::Test {
 protected:
  void SetUp() override { this->ld2450_.set_uart_parent(&this->mock_uart_); }

  MockUARTComponent mock_uart_;
  ZoneTestableLD2450 ld2450_;
};

TEST_F(LD2450ZoneAckTest, CmdFooterBytesInZonePayloadDoNotTruncateAck) {
  for (uint8_t zone = 0; zone < MAX_ZONES; zone++) {
    std::vector<uint8_t> frame = {0xFD, 0xFC, 0xFB, 0xFA, 0x1E, 0x00, 0xC1, 0x01, 0x00, 0x00, 0x01, 0x00};
    frame.resize(36, 0);
    const size_t start = 12 + zone * 8;
    // x1=772 and y1=258 contain the command footer before x2=1500 and y2=3000.
    static constexpr uint8_t COORDINATES[] = {0x04, 0x03, 0x02, 0x01, 0xDC, 0x05, 0xB8, 0x0B};
    std::memcpy(frame.data() + start, COORDINATES, sizeof(COORDINATES));
    frame.insert(frame.end(), {0x04, 0x03, 0x02, 0x01});
    std::memset(this->ld2450_.buffer_data_, 0xEE, MAX_LINE_LENGTH);

    const size_t split = start + 4;
    this->ld2450_.feed(std::vector<uint8_t>(frame.begin(), frame.begin() + split));
    EXPECT_EQ(this->ld2450_.buffer_pos_, split) << "Zone " << static_cast<int>(zone);
    this->ld2450_.feed(std::vector<uint8_t>(frame.begin() + split, frame.end()));
    EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
    for (uint8_t i = 0; i < MAX_ZONES; i++) {
      EXPECT_EQ(this->ld2450_.zone_config_[i].x1, i == zone ? 772 : 0);
      EXPECT_EQ(this->ld2450_.zone_config_[i].y1, i == zone ? 258 : 0);
      EXPECT_EQ(this->ld2450_.zone_config_[i].x2, i == zone ? 1500 : 0);
      EXPECT_EQ(this->ld2450_.zone_config_[i].y2, i == zone ? 3000 : 0);
    }
  }
}

TEST_F(LD2450ZoneAckTest, CompleteZoneAckWithoutEmbeddedFooter) {
  for (auto &zone : this->ld2450_.zone_config_) {
    zone.x1 = 100;
    zone.y1 = 200;
    zone.x2 = 300;
    zone.y2 = 400;
  }
  std::vector<uint8_t> frame = {0xFD, 0xFC, 0xFB, 0xFA, 0x1E, 0x00, 0xC1, 0x01, 0x00, 0x00, 0x01, 0x00};
  frame.resize(36, 0);
  frame.insert(frame.end(), {0x04, 0x03, 0x02, 0x01});
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  for (const auto &zone : this->ld2450_.zone_config_) {
    EXPECT_EQ(zone.x1, 0);
    EXPECT_EQ(zone.y1, 0);
    EXPECT_EQ(zone.x2, 0);
    EXPECT_EQ(zone.y2, 0);
  }
}

TEST_F(LD2450ZoneAckTest, FailedZoneQueryAckDoesNotWaitForCoordinates) {
  for (auto &zone : this->ld2450_.zone_config_) {
    zone.x1 = 100;
    zone.y1 = 200;
    zone.x2 = 300;
    zone.y2 = 400;
  }
  auto frame = make_ack_frame();
  frame[6] = 0xC1;
  frame[8] = 0x01;
  this->ld2450_.feed(frame);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  for (const auto &zone : this->ld2450_.zone_config_) {
    EXPECT_EQ(zone.x1, 100);
    EXPECT_EQ(zone.y1, 200);
    EXPECT_EQ(zone.x2, 300);
    EXPECT_EQ(zone.y2, 400);
  }
}

TEST_F(LD2450ZoneAckTest, TruncatedZoneAckThenCompleteZoneAckRecovers) {
  std::vector<uint8_t> truncated = {0xFD, 0xFC, 0xFB, 0xFA, 0x1E, 0x00, 0xC1, 0x01,
                                    0x00, 0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
  this->ld2450_.feed(truncated);
  std::vector<uint8_t> complete = {0xFD, 0xFC, 0xFB, 0xFA, 0x1E, 0x00, 0xC1, 0x01, 0x00, 0x00,
                                   0x01, 0x00, 0x64, 0x00, 0xC8, 0x00, 0xDC, 0x05, 0xB8, 0x0B};
  complete.resize(36, 0);
  complete.insert(complete.end(), {0x04, 0x03, 0x02, 0x01});
  this->ld2450_.feed(complete);
  EXPECT_EQ(this->ld2450_.buffer_pos_, 0);
  EXPECT_EQ(this->ld2450_.zone_config_[0].x1, 100);
  EXPECT_EQ(this->ld2450_.zone_config_[0].y1, 200);
  EXPECT_EQ(this->ld2450_.zone_config_[0].x2, 1500);
  EXPECT_EQ(this->ld2450_.zone_config_[0].y2, 3000);
}

}  // namespace esphome::ld2450::testing
