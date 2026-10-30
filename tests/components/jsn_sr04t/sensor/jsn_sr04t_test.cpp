#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/jsn_sr04t/jsn_sr04t.h"
#include "../common.h"

namespace esphome::jsn_sr04t::testing {

namespace {

std::vector<uint8_t> frame(uint16_t mm) {
  uint8_t hi = mm >> 8, lo = mm & 0xFF;
  return {0xFF, hi, lo, static_cast<uint8_t>(0xFF + hi + lo)};
}

class Jsnsr04tTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_model(JSN_SR04T);
    this->sut_.add_on_state_callback([this](float state) { this->published_.push_back(state); });
  }

  void feed_(const std::vector<uint8_t> &bytes) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    this->sut_.loop();
  }

  MockUARTComponent uart_;
  Jsnsr04tComponent sut_;
  std::vector<float> published_;
};

}  // namespace

TEST_F(Jsnsr04tTest, GoodFramePublishesMeters) {
  this->feed_(frame(1953));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.953f);
}

TEST_F(Jsnsr04tTest, BadChecksumIsDropped) {
  auto bad = frame(1953);
  bad[3] ^= 1;
  this->feed_(bad);
  EXPECT_TRUE(this->published_.empty());
  this->feed_(frame(500));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 0.5f);
}

TEST_F(Jsnsr04tTest, DistanceAtOrBelowMinimumIsDropped) {
  this->feed_(frame(250));
  EXPECT_TRUE(this->published_.empty());
}

TEST_F(Jsnsr04tTest, Ajsr04mHasLowerMinimum) {
  this->sut_.set_model(AJ_SR04M);
  this->feed_(frame(200));
  EXPECT_TRUE(this->published_.empty());
  this->feed_(frame(201));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 0.201f);
}

TEST_F(Jsnsr04tTest, GarbageBeforeHeaderIsSkipped) {
  this->feed_({0x01, 0x02, 0x7F});
  this->feed_(frame(1000));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.0f);
}

TEST_F(Jsnsr04tTest, SplitReadsAreJoined) {
  auto f = frame(1234);
  this->feed_({f[0], f[1]});
  EXPECT_TRUE(this->published_.empty());
  this->feed_({f[2], f[3]});
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.234f);
}

TEST_F(Jsnsr04tTest, BackToBackFrames) {
  auto a = frame(1000), b = frame(2000);
  a.insert(a.end(), b.begin(), b.end());
  this->feed_(a);
  ASSERT_EQ(this->published_.size(), 2u);
  EXPECT_FLOAT_EQ(this->published_[1], 2.0f);
}

// A rejected frame is dropped whole; the parser does not look for a header inside it.
TEST_F(Jsnsr04tTest, StrayHeaderByteLosesFollowingFrame) {
  auto f = frame(1000);
  f.insert(f.begin(), 0xFF);
  this->feed_(f);
  EXPECT_TRUE(this->published_.empty());
  this->feed_(frame(1500));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.5f);
}

}  // namespace esphome::jsn_sr04t::testing
