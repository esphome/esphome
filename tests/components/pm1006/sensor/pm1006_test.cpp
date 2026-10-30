#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/pm1006/pm1006.h"
#include "esphome/components/sensor/sensor.h"
#include "../common.h"

namespace esphome::pm1006::testing {

namespace {

std::vector<uint8_t> frame(uint16_t pm25) {
  std::vector<uint8_t> f{0x16, 0x11, 0x0B, 0x00, 0x00, static_cast<uint8_t>(pm25 >> 8), static_cast<uint8_t>(pm25)};
  f.resize(19, 0x00);
  uint8_t sum = 0;
  for (uint8_t b : f)
    sum += b;
  f.push_back(static_cast<uint8_t>(-sum));
  return f;
}

class PM1006Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_pm_2_5_sensor(&this->pm_2_5_);
    this->pm_2_5_.add_on_state_callback([this](float state) { this->published_.push_back(state); });
  }

  void feed_(const std::vector<uint8_t> &bytes) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    this->sut_.loop();
  }

  MockUARTComponent uart_;
  PM1006Component sut_;
  sensor::Sensor pm_2_5_;
  std::vector<float> published_;
};

}  // namespace

TEST_F(PM1006Test, GoodFramePublishes) {
  this->feed_(frame(0x0123));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 0x0123);
}

TEST_F(PM1006Test, BadChecksumIsDropped) {
  auto bad = frame(42);
  bad[19] ^= 1;
  this->feed_(bad);
  EXPECT_TRUE(this->published_.empty());
  this->feed_(frame(43));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 43);
}

TEST_F(PM1006Test, GarbageBeforeHeaderIsSkipped) {
  this->feed_({0x00, 0xFF, 0x11, 0x0B});
  this->feed_(frame(17));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 17);
}

TEST_F(PM1006Test, BrokenHeaderIsSkipped) {
  this->feed_({0x16, 0x11, 0x00});
  this->feed_(frame(18));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 18);
}

TEST_F(PM1006Test, SplitReadsAreJoined) {
  auto f = frame(500);
  this->feed_(std::vector<uint8_t>(f.begin(), f.begin() + 2));
  this->feed_(std::vector<uint8_t>(f.begin() + 2, f.begin() + 11));
  EXPECT_TRUE(this->published_.empty());
  this->feed_(std::vector<uint8_t>(f.begin() + 11, f.end()));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 500);
}

TEST_F(PM1006Test, BackToBackFrames) {
  auto a = frame(1), b = frame(2);
  a.insert(a.end(), b.begin(), b.end());
  this->feed_(a);
  ASSERT_EQ(this->published_.size(), 2u);
  EXPECT_FLOAT_EQ(this->published_[1], 2);
}

// The byte that breaks a header is dropped, not checked as the start of a new header.
TEST_F(PM1006Test, RepeatedFirstHeaderByteLosesFrame) {
  auto f = frame(77);
  f.insert(f.begin(), 0x16);
  this->feed_(f);
  EXPECT_TRUE(this->published_.empty());
}

}  // namespace esphome::pm1006::testing
