#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

#include "esphome/components/audio_analyzer/sensor/audio_analyzer_sensor.h"
#include "esphome/core/application.h"

namespace esphome::audio_analyzer::testing {

static constexpr double TWO_PI = 6.283185307179586;

class FedAnalyzer : public AudioAnalyzer {
 public:
  FedAnalyzer() {
    this->set_attack(0);
    this->set_release(0);
    this->configure_analysis_(16000.0f);
  }
  void feed_frame(float amplitude) {
    for (uint32_t i = 0; i < FFT_SIZE; ++i) {
      this->push_sample_(amplitude * static_cast<float>(std::sin(TWO_PI * 1000.0 * i / 16000.0)));
    }
  }
  void release_if_idle() { this->release_if_idle_(); }
};

static void at(uint32_t now_ms) { LoopBlockingGuard clock{nullptr, nullptr, now_ms}; }

static float mean_square_db(float amplitude) { return 10.0f * log10f(amplitude * amplitude / 2.0f); }

class AudioAnalyzerSensorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.set_rms_sensor(&this->rms_);
    this->sensor_.set_band_sensor(9, &this->band_);
    this->sensor_.setup();
    at(1000);
  }
  FedAnalyzer analyzer_;
  AudioAnalyzerSensor sensor_{&this->analyzer_};
  sensor::Sensor rms_;
  sensor::Sensor band_;
};

TEST_F(AudioAnalyzerSensorTest, PublishesTheLoudestFrameSinceTheLastUpdate) {
  this->analyzer_.feed_frame(0.5f);
  this->analyzer_.feed_frame(0.1f);
  this->sensor_.update();
  EXPECT_NEAR(this->rms_.state, mean_square_db(0.5f), 0.1f);
  EXPECT_FALSE(std::isnan(this->band_.state));
  // The hold restarts after each update
  this->analyzer_.feed_frame(0.1f);
  this->sensor_.update();
  EXPECT_NEAR(this->rms_.state, mean_square_db(0.1f), 0.1f);
}

TEST_F(AudioAnalyzerSensorTest, UpdateBetweenFramesKeepsTheValue) {
  // An update interval shorter than a frame must not flap to unknown while audio is analyzed
  this->analyzer_.feed_frame(0.5f);
  this->sensor_.update();
  at(1020);
  this->sensor_.update();
  EXPECT_NEAR(this->rms_.state, mean_square_db(0.5f), 0.1f);
}

TEST_F(AudioAnalyzerSensorTest, UnknownOnceTheAnalyzerIsIdle) {
  this->analyzer_.feed_frame(0.5f);
  this->sensor_.update();
  int publishes = 0;
  this->rms_.add_on_state_callback([&publishes](float) { ++publishes; });
  at(1000 + 100);
  this->analyzer_.release_if_idle();
  ASSERT_FALSE(this->analyzer_.is_active());
  this->sensor_.update();
  EXPECT_TRUE(std::isnan(this->rms_.state));
  EXPECT_TRUE(std::isnan(this->band_.state));
  this->sensor_.update();
  EXPECT_EQ(publishes, 1);
}

}  // namespace esphome::audio_analyzer::testing
