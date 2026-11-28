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
}  // namespace esphome::micro_wake_word::testing
#endif
