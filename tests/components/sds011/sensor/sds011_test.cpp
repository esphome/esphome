#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/sds011/sds011.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/application.h"
#include "../common.h"

namespace esphome::sds011::testing {

namespace {

// PM values in tenths of a µg/m³
std::vector<uint8_t> frame(uint16_t pm25, uint16_t pm10) {
  std::vector<uint8_t> f{0xAA,
                         0xC0,
                         static_cast<uint8_t>(pm25),
                         static_cast<uint8_t>(pm25 >> 8),
                         static_cast<uint8_t>(pm10),
                         static_cast<uint8_t>(pm10 >> 8),
                         0x12,
                         0x34};
  uint8_t sum = 0;
  for (size_t i = 2; i < 8; i++)
    sum += f[i];
  f.push_back(sum);
  f.push_back(0xAB);
  return f;
}

class SDS011Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_rx_mode_only(true);
    this->sut_.set_pm_2_5_sensor(&this->pm_2_5_);
    this->sut_.set_pm_10_0_sensor(&this->pm_10_0_);
    this->pm_2_5_.add_on_state_callback([this](float state) { this->pm_2_5_published_.push_back(state); });
    this->pm_10_0_.add_on_state_callback([this](float state) { this->pm_10_0_published_.push_back(state); });
  }

  void feed_(const std::vector<uint8_t> &bytes, uint32_t now = 0) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    LoopBlockingGuard{nullptr, nullptr, now};
    this->sut_.loop();
  }

  MockUARTComponent uart_;
  SDS011Component sut_;
  sensor::Sensor pm_2_5_;
  sensor::Sensor pm_10_0_;
  std::vector<float> pm_2_5_published_;
  std::vector<float> pm_10_0_published_;
};

}  // namespace

TEST_F(SDS011Test, GoodFramePublishes) {
  this->feed_(frame(123, 456));
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  ASSERT_EQ(this->pm_10_0_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 12.3f);
  EXPECT_FLOAT_EQ(this->pm_10_0_published_[0], 45.6f);
}

TEST_F(SDS011Test, AllZeroValuesAreNotPublished) {
  this->feed_(frame(0, 0));
  EXPECT_TRUE(this->pm_2_5_published_.empty());
}

TEST_F(SDS011Test, BadChecksumIsDropped) {
  auto bad = frame(123, 456);
  bad[8] ^= 1;
  this->feed_(bad);
  EXPECT_TRUE(this->pm_2_5_published_.empty());
  this->feed_(frame(10, 20));
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 1.0f);
}

TEST_F(SDS011Test, BadTailIsDropped) {
  auto bad = frame(123, 456);
  bad[9] = 0x00;
  this->feed_(bad);
  EXPECT_TRUE(this->pm_2_5_published_.empty());
  this->feed_(frame(10, 20));
  EXPECT_EQ(this->pm_2_5_published_.size(), 1u);
}

TEST_F(SDS011Test, OtherCommandIsIgnored) {
  auto reply = frame(123, 456);
  reply[1] = 0xC5;
  this->feed_(reply);
  EXPECT_TRUE(this->pm_2_5_published_.empty());
  this->feed_(frame(10, 20));
  EXPECT_EQ(this->pm_2_5_published_.size(), 1u);
}

TEST_F(SDS011Test, GarbageBeforeHeaderIsSkipped) {
  this->feed_({0x01, 0xC0, 0xAB});
  this->feed_(frame(50, 60));
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_10_0_published_[0], 6.0f);
}

TEST_F(SDS011Test, SplitReadsAreJoined) {
  auto f = frame(70, 80);
  this->feed_(std::vector<uint8_t>(f.begin(), f.begin() + 1), 0);
  this->feed_(std::vector<uint8_t>(f.begin() + 1, f.begin() + 6), 100);
  EXPECT_TRUE(this->pm_2_5_published_.empty());
  this->feed_(std::vector<uint8_t>(f.begin() + 6, f.end()), 200);
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 7.0f);
}

// A frame idle for 500 ms or more is dropped, and its tail is not mistaken for a frame
TEST_F(SDS011Test, GapDropsPartialFrame) {
  auto f = frame(70, 80);
  this->feed_(std::vector<uint8_t>(f.begin(), f.begin() + 5), 1000);
  this->feed_(std::vector<uint8_t>(f.begin() + 5, f.end()), 1500);
  EXPECT_TRUE(this->pm_2_5_published_.empty());
  this->feed_(frame(90, 100), 1600);
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 9.0f);
}

TEST_F(SDS011Test, ShortGapKeepsPartialFrame) {
  auto f = frame(70, 80);
  this->feed_(std::vector<uint8_t>(f.begin(), f.begin() + 5), 1000);
  this->feed_(std::vector<uint8_t>(f.begin() + 5, f.end()), 1499);
  EXPECT_EQ(this->pm_2_5_published_.size(), 1u);
}

TEST_F(SDS011Test, BackToBackFrames) {
  auto a = frame(1, 2), b = frame(3, 4);
  a.insert(a.end(), b.begin(), b.end());
  this->feed_(a);
  ASSERT_EQ(this->pm_2_5_published_.size(), 2u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[1], 0.3f);
}

// The byte that breaks a header is checked again as the start of a new header.
TEST_F(SDS011Test, RepeatedFirstHeaderByteKeepsFrame) {
  auto f = frame(70, 80);
  f.insert(f.begin(), 0xAA);
  this->feed_(f);
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 7.0f);
}

// A cut-off frame fails the checks; the frame that started inside it is still found.
TEST_F(SDS011Test, FrameInsideRejectedFrameIsFound) {
  auto cut = frame(1, 2);
  cut.resize(5);
  auto f = frame(70, 80);
  cut.insert(cut.end(), f.begin(), f.end());
  this->feed_(cut);
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 7.0f);
}

}  // namespace esphome::sds011::testing
