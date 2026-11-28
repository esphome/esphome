#ifdef USE_HOST
#include <gtest/gtest.h>

#include <array>
#include "esphome/components/host/preferences.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

// Compile the production model owner; only its external inference engine uses __sdk.
#define USE_ESP32
// NOLINTNEXTLINE(bugprone-suspicious-include): exercise the ESP-only owner with the host inference double.
#include "esphome/components/micro_wake_word/streaming_model.cpp"

namespace esphome::micro_wake_word {
// Runtime-downloaded model storage is outside these compiled-in model tests.
const uint8_t *ModelData::get_model_pointer() const { return nullptr; }
}  // namespace esphome::micro_wake_word

namespace esphome::micro_wake_word::testing {
class StreamingReloadTest : public ::testing::TestWithParam<unsigned> {
 protected:
  void SetUp() override {
    host::setup_preferences();
    invocations.clear();
    output_type = kTfLiteUInt8;
    output_classes = 1;
    input_slices = 3;
    output_bytes = {};
  }
  static void feed(WakeWordModel &model, int8_t value) {
    std::array<int8_t, PREPROCESSOR_FEATURE_SIZE> features;
    features.fill(value);
    ASSERT_TRUE(model.perform_streaming_inference(features.data()));
  }
  static void expect_rows(const std::array<int8_t, 120> &input, std::array<int8_t, 3> values) {
    for (size_t row = 0; row < values.size(); ++row)
      for (size_t bin = 0; bin < PREPROCESSOR_FEATURE_SIZE; ++bin)
        EXPECT_EQ(input[row * PREPROCESSOR_FEATURE_SIZE + bin], values[row]);
  }
  static constexpr uint8_t MODEL_DATA[]{0};
};

TEST_P(StreamingReloadTest, PublicDisableAndEnableRequireAnEntireNewStride) {
  WakeWordModel model("reload", MODEL_DATA, sizeof(MODEL_DATA), 229, 5, "example", 1024, true, false);
  this->feed(model, -10);
  this->feed(model, -11);
  this->feed(model, -12);
  ASSERT_EQ(invocations.size(), 1u);
  for (unsigned slice = 0; slice < this->GetParam(); ++slice)
    this->feed(model, -20 - slice);
  ASSERT_TRUE(model.get_unprocessed_probability_status());

  // The inference task checks each new score even during the initial detection warm-up.
  EXPECT_FALSE(model.determine_detected().detected);
  ASSERT_TRUE(model.get_unprocessed_probability_status());
  model.disable();
  this->feed(model, -30);
  EXPECT_FALSE(model.get_unprocessed_probability_status());
  model.enable();
  this->feed(model, 17);
  EXPECT_EQ(invocations.size(), 1u);
  EXPECT_FALSE(model.get_unprocessed_probability_status());
  this->feed(model, 34);
  EXPECT_EQ(invocations.size(), 1u);
  EXPECT_FALSE(model.get_unprocessed_probability_status());
  this->feed(model, 51);
  ASSERT_EQ(invocations.size(), 2u);
  EXPECT_TRUE(model.get_unprocessed_probability_status());
  this->expect_rows(invocations.back(), {17, 34, 51});
}

TEST_P(StreamingReloadTest, AProbabilityResetRetainsTheCurrentInterpreterStride) {
  WakeWordModel model("continuous", MODEL_DATA, sizeof(MODEL_DATA), 229, 5, "example", 1024, true, false);
  this->feed(model, -10);
  this->feed(model, -11);
  this->feed(model, -12);
  for (unsigned slice = 0; slice < this->GetParam(); ++slice)
    this->feed(model, -20 - slice);
  model.reset_probabilities();
  for (unsigned slice = this->GetParam(); slice < 3; ++slice)
    this->feed(model, -20 - slice);
  ASSERT_EQ(invocations.size(), 2u);
  this->expect_rows(invocations.back(), {-20, -21, -22});
}

INSTANTIATE_TEST_SUITE_P(StridePhase, StreamingReloadTest, ::testing::Values(0u, 1u, 2u));

using ModelOptionsTest = StreamingReloadTest;

TEST_F(ModelOptionsTest, AnInferenceIntervalSlidesTheWindow) {
  WakeWordModel model("sliding", MODEL_DATA, sizeof(MODEL_DATA), 229, 5, "example", 1024, true, false);
  model.set_inference_interval(1);
  this->feed(model, 1);
  this->feed(model, 2);
  EXPECT_TRUE(invocations.empty());
  this->feed(model, 3);
  this->feed(model, 4);
  ASSERT_EQ(invocations.size(), 2u);
  this->expect_rows(invocations[0], {1, 2, 3});
  this->expect_rows(invocations[1], {2, 3, 4});
}

TEST_F(ModelOptionsTest, AReloadStartsWithAnEmptyWindow) {
  WakeWordModel model("restart", MODEL_DATA, sizeof(MODEL_DATA), 229, 5, "example", 1024, true, false);
  model.set_inference_interval(2);
  for (int8_t value = 1; value <= 5; ++value)
    this->feed(model, value);
  ASSERT_EQ(invocations.size(), 2u);
  this->expect_rows(invocations[1], {3, 4, 5});
  model.disable();
  this->feed(model, 6);
  model.enable();
  this->feed(model, 7);
  this->feed(model, 8);
  EXPECT_EQ(invocations.size(), 2u);
  this->feed(model, 9);
  ASSERT_EQ(invocations.size(), 3u);
  this->expect_rows(invocations[2], {7, 8, 9});
}

TEST_F(ModelOptionsTest, TheSelectedClassOfAnInt8OutputIsTheProbability) {
  output_type = kTfLiteInt8;
  output_classes = 2;
  // The raw bytes of int8 -128, a probability of 0, for both classes.
  output_bytes = {128, 128};
  WakeWordModel model("classes", MODEL_DATA, sizeof(MODEL_DATA), 229, 1, "example", 1024, true, false);
  model.set_output_index(1);
  // Past the warm-up, which only low probabilities count down.
  for (unsigned slice = 0; slice < MIN_SLICES_BEFORE_DETECTION + 2u; ++slice)
    this->feed(model, 0);
  EXPECT_EQ(model.determine_detected().max_probability, 0);
  output_bytes = {128, 127};  // class 1 at int8 127
  for (unsigned slice = 0; slice < 3u; ++slice)
    this->feed(model, 0);
  const DetectionEvent event = model.determine_detected();
  EXPECT_EQ(event.max_probability, 255);
  EXPECT_TRUE(event.detected);
}

TEST_F(ModelOptionsTest, AClassTheModelDoesNotHaveFailsTheLoad) {
  WakeWordModel model("missing", MODEL_DATA, sizeof(MODEL_DATA), 229, 5, "example", 1024, true, false);
  model.set_output_index(1);
  std::array<int8_t, PREPROCESSOR_FEATURE_SIZE> features{};
  EXPECT_FALSE(model.perform_streaming_inference(features.data()));
}
TEST_F(ModelOptionsTest, AnIntervalLongerThanTheWindowSkipsSlices) {
  WakeWordModel model("sparse", MODEL_DATA, sizeof(MODEL_DATA), 229, 5, "example", 1024, true, false);
  model.set_inference_interval(5);
  for (int8_t value = 1; value <= 11; ++value)
    this->feed(model, value);
  ASSERT_EQ(invocations.size(), 2u);
  this->expect_rows(invocations[0], {3, 4, 5});
  this->expect_rows(invocations[1], {8, 9, 10});
}

TEST_F(ModelOptionsTest, AnInputOfMoreThan255SlicesFailsTheLoad) {
  input_slices = 256;
  WakeWordModel model("long", MODEL_DATA, sizeof(MODEL_DATA), 229, 5, "example", 1024, true, false);
  model.set_inference_interval(1);
  std::array<int8_t, PREPROCESSOR_FEATURE_SIZE> features{};
  EXPECT_FALSE(model.perform_streaming_inference(features.data()));
}
}  // namespace esphome::micro_wake_word::testing
#endif
