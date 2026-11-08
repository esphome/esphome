#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/sm300d2/sm300d2.h"
#include "esphome/components/sensor/sensor.h"
#include "../common.h"

namespace esphome::sm300d2::testing {

namespace {

// A 17-byte frame as the sensor sends it on its own: the given CO2 reading, the given humidity plus 0.2 %, and the
// other readings of the example in the module's documentation. frame(2300) is that example (48.2 %), checksum 0x18.
std::vector<uint8_t> frame(uint16_t co2, uint8_t humidity = 48) {
  const uint8_t high = co2 >> 8;
  const uint8_t low = co2 & 0xFF;
  std::vector<uint8_t> f{
      0x3C,     0x02,  // header
      high,     low,   // CO2 in ppm
      0x00,     0x79,  // formaldehyde 121 µg/m³
      0x01,     0xD7,  // TVOC 471 µg/m³
      0x00,     0x13,  // PM2.5 19 µg/m³
      0x00,     0x22,  // PM10 34 µg/m³
      0x1B,     0x03,  // 27.3 °C
      humidity, 0x02,  // humidity in percent, then tenths
  };
  // Checksum: the low byte of the sum of the 16 bytes above
  uint8_t sum = 0;
  for (uint8_t b : f)
    sum += b;
  f.push_back(sum);
  return f;
}

class SM300D2Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_co2_sensor(&this->co2_);
    this->sut_.set_temperature_sensor(&this->temperature_);
    this->sut_.set_humidity_sensor(&this->humidity_);
    this->co2_.add_on_state_callback([this](float state) { this->co2_published_.push_back(state); });
  }

  // Bytes arrive on the UART, then the main loop runs loop() once
  void receive_(const std::vector<uint8_t> &bytes) {
    this->uart_.rx.insert(this->uart_.rx.end(), bytes.begin(), bytes.end());
    this->sut_.loop();
  }

  MockUARTComponent uart_;
  SM300D2Sensor sut_;
  sensor::Sensor co2_;
  sensor::Sensor temperature_;
  sensor::Sensor humidity_;
  std::vector<float> co2_published_;
};

}  // namespace

TEST_F(SM300D2Test, FrameIsPublishedAtTheNextUpdate) {
  this->receive_(frame(2300));
  this->sut_.update();
  ASSERT_EQ(this->co2_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->co2_published_[0], 2300);
  EXPECT_FLOAT_EQ(this->temperature_.state, 27.3f);
  EXPECT_FLOAT_EQ(this->humidity_.state, 48.2f);
  EXPECT_FALSE(this->sut_.status_has_warning());
}

// The sensor sends a frame about once a second and update() runs every 60 s: 60 frames of 17 bytes are 1020 bytes,
// four times the default 256-byte RX buffer. loop() takes each frame off the UART as it arrives.
TEST_F(SM300D2Test, LoopTakesEachFrameOffTheUart) {
  this->receive_(frame(400));
  EXPECT_TRUE(this->uart_.rx.empty());
}

// Three frames, one second apart, between two polls: update() publishes the newest.
TEST_F(SM300D2Test, UpdatePublishesTheNewestFrame) {
  this->receive_(frame(400));
  this->receive_(frame(410));
  this->receive_(frame(420));
  this->sut_.update();
  ASSERT_EQ(this->co2_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->co2_published_[0], 420);
}

// At 9600 baud a frame takes 18 ms, longer than one 16 ms loop pass, so it often arrives in two parts.
TEST_F(SM300D2Test, FrameInTwoPartsIsJoined) {
  const std::vector<uint8_t> f = frame(400);
  this->receive_(std::vector<uint8_t>(f.begin(), f.begin() + 8));
  this->receive_(std::vector<uint8_t>(f.begin() + 8, f.end()));
  this->sut_.update();
  ASSERT_EQ(this->co2_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->co2_published_[0], 400);
}

// Each frame is published once; a poll without a new frame since the last one is a warning.
TEST_F(SM300D2Test, UpdateWithoutNewFrameWarns) {
  this->receive_(frame(400));
  this->sut_.update();
  EXPECT_FALSE(this->sut_.status_has_warning());
  this->sut_.update();
  EXPECT_EQ(this->co2_published_.size(), 1u);
  EXPECT_TRUE(this->sut_.status_has_warning());
}

TEST_F(SM300D2Test, BadChecksumIsNotPublished) {
  std::vector<uint8_t> bad = frame(400);
  bad[16] += 1;  // checksum off by one
  this->receive_(bad);
  this->sut_.update();
  EXPECT_TRUE(this->co2_published_.empty());
  EXPECT_TRUE(this->sut_.status_has_warning());
}

// A frame with a bad checksum is dropped as it arrives, so the poll publishes the good frame before it.
TEST_F(SM300D2Test, BadFrameDoesNotReplaceTheGoodOne) {
  std::vector<uint8_t> bad = frame(410);
  bad[16] += 1;  // checksum off by one
  this->receive_(frame(400));
  this->receive_(bad);
  this->sut_.update();
  ASSERT_EQ(this->co2_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->co2_published_[0], 400);
}

// At 60.2 % humidity a frame holds 0x3C 0x02, the same two bytes as the header. When reading starts in the middle of
// such a frame, the reader first takes these two bytes for the header; the 17 bytes from there fail the checksum, and
// the reader finds the real header of the next frame in them.
TEST_F(SM300D2Test, ReaderFindsTheRealHeaderAfterAFalseOne) {
  const std::vector<uint8_t> first = frame(400, 60);
  this->receive_(std::vector<uint8_t>(first.end() - 9, first.end()));  // only the last 9 of its 17 bytes arrive
  this->receive_(frame(410, 60));
  this->sut_.update();
  ASSERT_EQ(this->co2_published_.size(), 1u);
  EXPECT_FLOAT_EQ(this->co2_published_[0], 410);
}

}  // namespace esphome::sm300d2::testing
