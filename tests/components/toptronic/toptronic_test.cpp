#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/toptronic/toptronic.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome::toptronic::testing {

namespace {

static constexpr uint32_t START_ID = 0x1Fu << 24;
static constexpr uint32_t CONT_ID = 0x10u << 24;
static constexpr uint8_t TAG = 0x2A;

class NullCanbus : public canbus::Canbus {
 protected:
  bool setup_internal() override { return true; }
  canbus::Error send_message(canbus::CanFrame *) override { return canbus::ERROR_OK; }
  canbus::Error read_message(canbus::CanFrame *) override { return canbus::ERROR_NOMSG; }
};

class TestableTopTronic : public TopTronic {
 public:
  explicit TestableTopTronic(canbus::Canbus *bus) : TopTronic(bus, 0x208) {}

  // Feeds one frame; returns the completed payload, or an empty vector when nothing completed
  std::vector<uint8_t> feed(uint32_t can_id, const std::vector<uint8_t> &frame) {
    const uint8_t *msg;
    size_t len;
    if (!this->parse_frame_(frame.data(), frame.size(), can_id, &msg, &len))
      return {};
    return std::vector<uint8_t>(msg, msg + len);
  }

  size_t live_slots(uint32_t now) const {
    size_t n = 0;
    for (const auto &s : this->slots_)
      n += s.is_live(now) ? 1 : 0;
    return n;
  }
};

class TopTronicTest : public ::testing::Test {
 protected:
  void set_now(uint32_t now) {
    this->now_ = now;
    LoopBlockingGuard{nullptr, nullptr, now};
  }

  // Splits payload + CRC into a start frame and continuation frames
  static std::vector<std::vector<uint8_t>> split(std::vector<uint8_t> payload, bool good_crc = true) {
    uint16_t crc = crc16(payload.data(), payload.size(), 0x600D, 0x8408);
    if (!good_crc)
      crc ^= 1;
    payload.push_back(crc >> 8);
    payload.push_back(crc & 0xFF);
    std::vector<std::vector<uint8_t>> frames;
    size_t pos = std::min<size_t>(6, payload.size());
    frames.push_back({0, TAG});
    frames[0].insert(frames[0].end(), payload.begin(), payload.begin() + pos);
    while (pos < payload.size()) {
      size_t n = std::min<size_t>(7, payload.size() - pos);
      std::vector<uint8_t> f{TAG};
      f.insert(f.end(), payload.begin() + pos, payload.begin() + pos + n);
      frames.push_back(f);
      pos += n;
    }
    frames[0][0] = frames.size() << 3;
    return frames;
  }

  NullCanbus bus_;
  TestableTopTronic hub_{&this->bus_};
  uint32_t now_{1000};
};

static const std::vector<uint8_t> MESSAGE{0x42, 0x01, 0x02, 0x00, 0x10, 0xAA, 0xBB, 0xCC, 0xDD};

}  // namespace

TEST_F(TopTronicTest, SingleFrameMessageIsPassedThrough) {
  this->set_now(1000);
  EXPECT_EQ(this->hub_.feed(START_ID, {0x00, 0x42, 0x01, 0x02, 0x00, 0x10, 0x07}),
            (std::vector<uint8_t>{0x42, 0x01, 0x02, 0x00, 0x10, 0x07}));
}

TEST_F(TopTronicTest, MultiFrameMessageWithValidCrcCompletes) {
  this->set_now(1000);
  auto frames = split(MESSAGE);
  ASSERT_EQ(frames.size(), 2u);
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 1u);
  EXPECT_EQ(this->hub_.feed(CONT_ID, frames[1]), MESSAGE);
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
}

TEST_F(TopTronicTest, BadCrcIsDroppedAndFreesTheSlot) {
  this->set_now(1000);
  auto frames = split(MESSAGE, false);
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  EXPECT_TRUE(this->hub_.feed(CONT_ID, frames[1]).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
}

TEST_F(TopTronicTest, StaleSlotIsNotCompletedAndIsReclaimed) {
  this->set_now(1000);
  auto frames = split(MESSAGE);
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  this->set_now(7000);
  EXPECT_TRUE(this->hub_.feed(CONT_ID, frames[1]).empty());
  EXPECT_EQ(this->hub_.live_slots(7000), 0u);
  // A fresh start with the same tag reuses the reclaimed slot and completes
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  EXPECT_EQ(this->hub_.feed(CONT_ID, frames[1]), MESSAGE);
}

TEST_F(TopTronicTest, OldestSlotIsEvictedWhenAllAreBusy) {
  this->set_now(1000);
  for (uint8_t tag : {1, 2, 3}) {
    EXPECT_TRUE(this->hub_.feed(START_ID, {3 << 3, tag, 0x42}).empty());
    this->set_now(this->now_ + 10);
  }
  // Tag 1 was the oldest, so its continuation no longer matches a slot; tag 2 still does
  EXPECT_TRUE(this->hub_.feed(CONT_ID, {1, 0x00}).empty());
  EXPECT_EQ(this->hub_.live_slots(this->now_), 2u);
  EXPECT_TRUE(this->hub_.feed(CONT_ID, {2, 0x00}).empty());
  EXPECT_EQ(this->hub_.live_slots(this->now_), 2u);
}

TEST_F(TopTronicTest, FramesBeyondTheBufferAreDropped) {
  this->set_now(1000);
  // 31 frames announced, but oversized frames overflow the buffer before the count runs out
  EXPECT_TRUE(this->hub_.feed(START_ID, {31 << 3, TAG, 0x42}).empty());
  std::vector<uint8_t> big(100, 0x55);
  big[0] = TAG;
  EXPECT_TRUE(this->hub_.feed(CONT_ID, big).empty());
  EXPECT_TRUE(this->hub_.feed(CONT_ID, big).empty());
  EXPECT_TRUE(this->hub_.feed(CONT_ID, big).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
}

TEST_F(TopTronicTest, FrameCountOfOneIsRejected) {
  this->set_now(1000);
  EXPECT_TRUE(this->hub_.feed(START_ID, {1 << 3, TAG, 0x42}).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
}

}  // namespace esphome::toptronic::testing
