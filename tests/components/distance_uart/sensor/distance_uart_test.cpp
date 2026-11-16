#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "../common.h"
#include "esphome/components/distance_uart/distance_uart.h"

namespace esphome::distance_uart::testing {

namespace {

std::vector<uint8_t> frame(uint16_t mm) {
  uint8_t hi = mm >> 8, lo = mm & 0xFF;
  return {0xFF, hi, lo, static_cast<uint8_t>(0xFF + hi + lo)};
}

class DistanceUARTTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_mode(DistanceUARTMode::DISTANCE_UART_MODE_AUTO);
    this->sut_.set_publish_mode(DistanceUARTPublishMode::DISTANCE_UART_PUBLISH_MODE_IMMEDIATE);
    this->sut_.set_blind_zone(0.28f);
    this->sut_.set_max_range(7.5f);
    this->sut_.add_on_state_callback([this](float state) { this->published_.push_back(state); });
  }

  void feed_(const std::vector<uint8_t> &bytes) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    this->sut_.loop();
  }

  MockUARTComponent uart_;
  DistanceUARTSensor sut_;
  std::vector<float> published_;
};

}  // namespace

TEST_F(DistanceUARTTest, GoodFramePublishesMeters) {
  this->feed_(frame(1953));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.953f);
}

TEST_F(DistanceUARTTest, BadChecksumIsDropped) {
  auto bad = frame(1953);
  bad[3] ^= 1;
  this->feed_(bad);
  EXPECT_TRUE(this->published_.empty());
  this->feed_(frame(500));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 0.5f);
}

TEST_F(DistanceUARTTest, StrayHeaderByteResyncsAndRecoversFollowingFrame) {
  auto f = frame(1000);
  f.insert(f.begin(), 0xFF);
  this->feed_(f);
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.0f);
}

TEST_F(DistanceUARTTest, BlindZonePublishesNanInImmediateMode) {
  this->feed_(frame(280));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_TRUE(std::isnan(this->published_[0]));
}

TEST_F(DistanceUARTTest, BlindZonePublishesNanOnUpdateInIntervalMode) {
  this->sut_.set_publish_mode(DistanceUARTPublishMode::DISTANCE_UART_PUBLISH_MODE_INTERVAL);
  this->feed_(frame(1000));
  EXPECT_TRUE(this->published_.empty());
  this->sut_.update();
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.0f);

  // Reading inside blind zone should publish NAN on next update()
  this->feed_(frame(280));
  this->sut_.update();
  ASSERT_EQ(this->published_.size(), 2u);
  EXPECT_TRUE(std::isnan(this->published_[1]));
}

TEST_F(DistanceUARTTest, MaxRangePublishesNanInImmediateAndIntervalModes) {
  this->feed_(frame(8000));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_TRUE(std::isnan(this->published_[0]));

  this->sut_.set_publish_mode(DistanceUARTPublishMode::DISTANCE_UART_PUBLISH_MODE_INTERVAL);
  this->feed_(frame(8000));
  this->sut_.update();
  ASSERT_EQ(this->published_.size(), 2u);
  EXPECT_TRUE(std::isnan(this->published_[1]));
}

TEST_F(DistanceUARTTest, ControlledModeWritesTriggerByteAndPublishesImmediately) {
  this->sut_.set_mode(DistanceUARTMode::DISTANCE_UART_MODE_CONTROLLED);
  this->sut_.set_publish_mode(DistanceUARTPublishMode::DISTANCE_UART_PUBLISH_MODE_INTERVAL);
  this->sut_.update();
  ASSERT_EQ(this->uart_.tx.size(), 1u);
  EXPECT_EQ(this->uart_.tx[0], 0x00);

  this->feed_(frame(1500));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.5f);
}

}  // namespace esphome::distance_uart::testing
