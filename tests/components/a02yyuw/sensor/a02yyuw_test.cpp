#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/a02yyuw/a02yyuw.h"
#include "../common.h"

namespace esphome::a02yyuw::testing {

namespace {

std::vector<uint8_t> frame(uint16_t mm) {
  uint8_t hi = mm >> 8, lo = mm & 0xFF;
  return {0xFF, hi, lo, static_cast<uint8_t>(0xFF + hi + lo)};
}

class A02yyuwTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.add_on_state_callback([this](float state) { this->published_.push_back(state); });
  }

  void feed_(const std::vector<uint8_t> &bytes) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    this->sut_.loop();
  }

  MockUARTComponent uart_;
  A02yyuwComponent sut_;
  std::vector<float> published_;
};

}  // namespace

TEST_F(A02yyuwTest, GoodFramePublishesMillimeters) {
  this->feed_(frame(1953));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1953.0f);
}

TEST_F(A02yyuwTest, BadChecksumIsDropped) {
  auto bad = frame(1953);
  bad[3] ^= 1;
  this->feed_(bad);
  EXPECT_TRUE(this->published_.empty());
  this->feed_(frame(500));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 500.0f);
}

TEST_F(A02yyuwTest, DistanceAtOrBelowMinimumIsDropped) {
  this->feed_(frame(30));
  EXPECT_TRUE(this->published_.empty());
}

TEST_F(A02yyuwTest, GarbageBeforeHeaderIsSkipped) {
  this->feed_({0x01, 0x02, 0x7F});
  this->feed_(frame(1000));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1000.0f);
}

TEST_F(A02yyuwTest, SplitReadsAreJoined) {
  auto f = frame(1234);
  this->feed_({f[0], f[1]});
  EXPECT_TRUE(this->published_.empty());
  this->feed_({f[2], f[3]});
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1234.0f);
}

TEST_F(A02yyuwTest, BackToBackFrames) {
  auto a = frame(1000), b = frame(2000);
  a.insert(a.end(), b.begin(), b.end());
  this->feed_(a);
  ASSERT_EQ(this->published_.size(), 2u);
  EXPECT_FLOAT_EQ(this->published_[1], 2000.0f);
}

// A rejected frame is dropped whole; the parser does not look for a header inside it.
TEST_F(A02yyuwTest, StrayHeaderByteLosesFollowingFrame) {
  auto f = frame(1000);
  f.insert(f.begin(), 0xFF);
  this->feed_(f);
  EXPECT_TRUE(this->published_.empty());
  this->feed_(frame(1500));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1500.0f);
}

}  // namespace esphome::a02yyuw::testing
