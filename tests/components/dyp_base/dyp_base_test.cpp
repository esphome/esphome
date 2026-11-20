#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "common.h"
#include "esphome/components/dyp_base/dyp_base.h"
#include "esphome/core/application.h"

namespace esphome::dyp_base::testing {

namespace {

std::vector<uint8_t> frame(uint16_t mm) {
  uint8_t hi = mm >> 8, lo = mm & 0xFF;
  return {0xFF, hi, lo, static_cast<uint8_t>(0xFF + hi + lo)};
}

class DYPSensorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    LoopBlockingGuard guard{&this->sut_, nullptr, 0};
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_mode(DYPMode::DYP_MODE_AUTO);
    this->sut_.set_publish_mode(DYPPublishMode::DYP_PUBLISH_MODE_IMMEDIATE);
    this->sut_.set_blind_zone(0.28f);
    this->sut_.set_max_range(7.5f);
    this->sut_.add_on_state_callback([this](float state) { this->published_.push_back(state); });
  }

  void feed_(const std::vector<uint8_t> &bytes, uint32_t now_ms = 0) {
    LoopBlockingGuard guard{&this->sut_, nullptr, now_ms};
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    this->sut_.loop();
  }

  MockUARTComponent uart_;
  DYPSensor sut_;
  std::vector<float> published_;
};

}  // namespace

TEST_F(DYPSensorTest, GoodFramePublishesMeters) {
  this->feed_(frame(1953));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.953f);
}

TEST_F(DYPSensorTest, BadChecksumIsDropped) {
  auto bad = frame(1953);
  bad[3] ^= 1;
  this->feed_(bad);
  EXPECT_TRUE(this->published_.empty());
  this->feed_(frame(500));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 0.5f);
}

TEST_F(DYPSensorTest, StrayHeaderByteResyncsAndRecoversFollowingFrame) {
  auto f = frame(1000);
  f.insert(f.begin(), 0xFF);
  this->feed_(f);
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.0f);
}

TEST_F(DYPSensorTest, TruncatedFrameTimesOutBeforeNextFrameInAutoMode) {
  // Truncated 3-byte frame {0xFF, 0x01, 0xFF}: if not expired before the next frame's
  // 0xFF header arrives, {0xFF, 0x01, 0xFF, 0xFF} would have a valid checksum and
  // falsely publish 0.511 m while dropping the following 1.500 m frame.
  this->feed_({0xFF, 0x01, 0xFF}, 10);
  EXPECT_TRUE(this->published_.empty());

  this->feed_(frame(1500), 100);
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.5f);
}

TEST_F(DYPSensorTest, ControlledModeResetsTruncatedFrameOnUpdate) {
  this->sut_.set_mode(DYPMode::DYP_MODE_CONTROLLED);
  this->sut_.update();
  this->feed_({0xFF, 0x01, 0xFF}, 10);
  EXPECT_TRUE(this->published_.empty());

  // Next update() triggers a new measurement and discards the stale partial frame
  // even before FRAME_TIMEOUT_MS elapses.
  this->sut_.update();
  this->feed_(frame(1500), 20);
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.5f);
}

TEST_F(DYPSensorTest, BlindZonePublishesNanInImmediateMode) {
  this->feed_(frame(280));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_TRUE(std::isnan(this->published_[0]));
}

TEST_F(DYPSensorTest, BlindZonePublishesNanOnUpdateInIntervalMode) {
  this->sut_.set_publish_mode(DYPPublishMode::DYP_PUBLISH_MODE_INTERVAL);
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

TEST_F(DYPSensorTest, MaxRangePublishesNanInImmediateAndIntervalModes) {
  this->feed_(frame(8000));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_TRUE(std::isnan(this->published_[0]));

  this->sut_.set_publish_mode(DYPPublishMode::DYP_PUBLISH_MODE_INTERVAL);
  this->feed_(frame(8000));
  this->sut_.update();
  ASSERT_EQ(this->published_.size(), 2u);
  EXPECT_TRUE(std::isnan(this->published_[1]));
}

TEST_F(DYPSensorTest, ControlledModeWritesTriggerByteAndPublishesImmediately) {
  this->sut_.set_mode(DYPMode::DYP_MODE_CONTROLLED);
  this->sut_.set_publish_mode(DYPPublishMode::DYP_PUBLISH_MODE_INTERVAL);
  this->sut_.update();
  ASSERT_EQ(this->uart_.tx.size(), 1u);
  EXPECT_EQ(this->uart_.tx[0], 0x00);

  this->feed_(frame(1500));
  ASSERT_EQ(this->published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->published_[0], 1.5f);
}

}  // namespace esphome::dyp_base::testing
