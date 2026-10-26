#include <gtest/gtest.h>

// The deprecation warning for vector callbacks is reported inside microphone.h. Remove in 2027.5.0.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "esphome/components/microphone/automation.h"
#include "esphome/components/microphone/microphone.h"
#pragma GCC diagnostic pop

#include <cstdint>
#include <vector>

namespace esphome::microphone::testing {

class TestMicrophone : public Microphone {
 public:
  void start() override {}
  void stop() override {}

  void publish(std::span<const uint8_t> data, const audio::AudioStreamInfo &stream_info, int64_t capture_time_us) {
    this->publish_audio_(data, stream_info, capture_time_us);
  }
  bool has_callbacks() const { return this->has_data_callbacks_(); }

  // Remove in 2027.5.0 with the deprecated DataCallbackManager::call(data)
  void set_stream_info(const audio::AudioStreamInfo &stream_info) { this->audio_stream_info_ = stream_info; }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  void publish_deprecated(const std::vector<uint8_t> &data) { this->data_callbacks_.call(data); }
#pragma GCC diagnostic pop
};

class RecordAction : public Action<const std::vector<uint8_t> &> {
 public:
  explicit RecordAction(std::vector<uint8_t> *out) : out_(out) {}

 protected:
  void play(const std::vector<uint8_t> &x) override { *this->out_ = x; }
  std::vector<uint8_t> *out_;
};

static const audio::AudioStreamInfo STEREO_48K(16, 2, 48000);
static const std::vector<uint8_t> SAMPLES{1, 2, 3, 4, 5, 6, 7, 8};

TEST(MicrophoneDataCallbacks, CallbackGetsDataFormatAndCaptureTime) {
  TestMicrophone mic;
  EXPECT_FALSE(mic.has_callbacks());

  std::vector<uint8_t> data;
  audio::AudioStreamInfo stream_info;
  int64_t capture_time_us = 0;
  mic.add_data_callback([&](const AudioCapture &capture) {
    data.assign(capture.data.begin(), capture.data.end());
    stream_info = capture.stream_info;
    capture_time_us = capture.capture_time_us;
  });
  EXPECT_TRUE(mic.has_callbacks());

  mic.publish(SAMPLES, STEREO_48K, 123456);
  EXPECT_EQ(data, SAMPLES);
  EXPECT_EQ(stream_info, STEREO_48K);
  EXPECT_EQ(capture_time_us, 123456);
}

TEST(MicrophoneDataCallbacks, MutedCallbackGetsZeros) {
  TestMicrophone mic;
  std::vector<uint8_t> data;
  mic.add_data_callback([&](const AudioCapture &capture) { data.assign(capture.data.begin(), capture.data.end()); });

  mic.set_mute_state(true);
  mic.publish(SAMPLES, STEREO_48K, 0);
  EXPECT_EQ(data, std::vector<uint8_t>(SAMPLES.size(), 0));
}

TEST(MicrophoneDataCallbacks, DataTriggerPassesACopy) {
  TestMicrophone mic;
  DataTrigger trigger(&mic);
  std::vector<uint8_t> data;
  Automation<const std::vector<uint8_t> &> automation(&trigger);
  RecordAction record(&data);
  automation.add_action(&record);

  std::vector<uint8_t> buffer = SAMPLES;
  mic.publish(buffer, STEREO_48K, 0);
  EXPECT_EQ(data, SAMPLES);
}

// Remove in 2027.5.0 with the deprecated vector callbacks; nothing else in the tree compiles that adapter
TEST(MicrophoneDataCallbacks, DeprecatedVectorCallbackGetsData) {
  TestMicrophone mic;
  std::vector<uint8_t> data;
  mic.add_data_callback([&](const std::vector<uint8_t> &x) { data = x; });

  std::vector<uint8_t> buffer = SAMPLES;
  mic.publish(buffer, STEREO_48K, 0);
  EXPECT_EQ(data, SAMPLES);
}

// Remove in 2027.5.0 with the deprecated DataCallbackManager::call(data)
TEST(MicrophoneDataCallbacks, DeprecatedCallUsesMicrophoneFormat) {
  TestMicrophone mic;
  mic.set_stream_info(STEREO_48K);
  std::vector<uint8_t> data;
  audio::AudioStreamInfo stream_info;
  int64_t capture_time_us = -1;
  mic.add_data_callback([&](const AudioCapture &capture) {
    data.assign(capture.data.begin(), capture.data.end());
    stream_info = capture.stream_info;
    capture_time_us = capture.capture_time_us;
  });

  mic.publish_deprecated(SAMPLES);
  EXPECT_EQ(data, SAMPLES);
  EXPECT_EQ(stream_info, STEREO_48K);
  EXPECT_EQ(capture_time_us, 0);
}

}  // namespace esphome::microphone::testing
