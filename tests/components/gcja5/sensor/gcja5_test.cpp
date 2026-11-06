#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/gcja5/gcja5.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/application.h"
#include "../common.h"

namespace esphome::gcja5::testing {

namespace {

std::vector<uint8_t> frame(uint32_t pm25, uint16_t pmc10) {
  std::vector<uint8_t> f(32, 0x00);
  f[0] = 0x02;
  f[5] = pm25;
  f[6] = pm25 >> 8;
  f[7] = pm25 >> 16;
  f[8] = pm25 >> 24;
  f[25] = pmc10;
  f[26] = pmc10 >> 8;
  uint8_t fcc = 0;
  for (size_t i = 1; i < 30; i++)
    fcc ^= f[i];
  f[30] = fcc;
  f[31] = 0x03;
  return f;
}

class GCJA5Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_pm_2_5_sensor(&this->pm_2_5_);
    this->sut_.set_pmc_10_0_sensor(&this->pmc_10_0_);
    this->pm_2_5_.add_on_state_callback([this](float state) { this->pm_2_5_published_.push_back(state); });
    this->pmc_10_0_.add_on_state_callback([this](float state) { this->pmc_10_0_published_.push_back(state); });
  }

  void feed_(const std::vector<uint8_t> &bytes, uint32_t now = 0) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    LoopBlockingGuard{nullptr, nullptr, now};
    this->sut_.loop();
  }

  MockUARTComponent uart_;
  GCJA5Component sut_;
  sensor::Sensor pm_2_5_;
  sensor::Sensor pmc_10_0_;
  std::vector<float> pm_2_5_published_;
  std::vector<float> pmc_10_0_published_;
};

}  // namespace

TEST_F(GCJA5Test, GoodFramePublishes) {
  this->feed_(frame(0x00012345, 0x0456));
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  ASSERT_EQ(this->pmc_10_0_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 0x00012345);
  EXPECT_FLOAT_EQ(this->pmc_10_0_published_[0], 0x0456);
}

TEST_F(GCJA5Test, BadChecksumIsDropped) {
  auto bad = frame(10, 20);
  bad[30] ^= 1;
  this->feed_(bad);
  EXPECT_TRUE(this->pm_2_5_published_.empty());
  EXPECT_TRUE(this->sut_.status_has_warning());
  this->feed_(frame(11, 21));
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 11);
  EXPECT_FALSE(this->sut_.status_has_warning());
}

TEST_F(GCJA5Test, BadEndByteIsDropped) {
  auto bad = frame(10, 20);
  bad[31] = 0x00;
  this->feed_(bad);
  EXPECT_TRUE(this->pm_2_5_published_.empty());
}

TEST_F(GCJA5Test, BadFrameAfterGoodFrameIsNotPublished) {
  this->feed_(frame(10, 20));
  auto bad = frame(99, 98);
  bad[30] ^= 1;
  this->feed_(bad);
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 10);
  EXPECT_TRUE(this->sut_.status_has_warning());
}

TEST_F(GCJA5Test, SplitReadsAreJoined) {
  auto f = frame(30, 40);
  this->feed_(std::vector<uint8_t>(f.begin(), f.begin() + 10), 0);
  this->feed_(std::vector<uint8_t>(f.begin() + 10, f.begin() + 20), 200);
  EXPECT_TRUE(this->pm_2_5_published_.empty());
  this->feed_(std::vector<uint8_t>(f.begin() + 20, f.end()), 400);
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 30);
}

TEST_F(GCJA5Test, BackToBackFrames) {
  auto a = frame(1, 2), b = frame(3, 4);
  a.insert(a.end(), b.begin(), b.end());
  this->feed_(a);
  ASSERT_EQ(this->pm_2_5_published_.size(), 2u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[1], 3);
}

TEST_F(GCJA5Test, GarbageBeforeHeaderIsSkipped) {
  std::vector<uint8_t> bytes{0x55};
  for (int i = 0; i < 3; i++) {
    auto f = frame(50 + i, 60);
    bytes.insert(bytes.end(), f.begin(), f.end());
  }
  this->feed_(bytes);
  ASSERT_EQ(this->pm_2_5_published_.size(), 3u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 50);
}

// A cut-off frame fails the checks; the frame that started inside it is still found.
TEST_F(GCJA5Test, FrameInsideRejectedFrameIsFound) {
  auto cut = frame(1, 2);
  cut.resize(12);
  auto f = frame(70, 80);
  cut.insert(cut.end(), f.begin(), f.end());
  this->feed_(cut);
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 70);
}

TEST_F(GCJA5Test, PauseDropsPartialFrame) {
  auto f = frame(30, 40);
  this->feed_(std::vector<uint8_t>(f.begin(), f.begin() + 10), 1000);
  this->feed_(frame(31, 41), 1500);
  ASSERT_EQ(this->pm_2_5_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->pm_2_5_published_[0], 31);
}

}  // namespace esphome::gcja5::testing
