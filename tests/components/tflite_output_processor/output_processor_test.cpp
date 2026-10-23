#include <gtest/gtest.h>

#include "esphome/components/tflite_output_processor/output_processor.h"

namespace esphome::tflite_output_processor::testing {

static constexpr float TOLERANCE = 1e-5f;

TEST(OutputProcessor, ArgmaxReturnsWinningClassAndItsScore) {
  const float output[3] = {0.1f, 2.5f, -0.4f};
  const ProcessedOutput result = process_output(output, 3, OutputProcessing::OUTPUT_PROCESSING_ARGMAX, 1.0f, false);
  EXPECT_FLOAT_EQ(result.value, 1.0f);
  EXPECT_FLOAT_EQ(result.confidence, 2.5f);
}

TEST(OutputProcessor, ScaleFactorDividesTheClassIndexInEveryMode) {
  const float output[3] = {0.1f, 2.5f, -0.4f};
  EXPECT_NEAR(process_output(output, 3, OutputProcessing::OUTPUT_PROCESSING_ARGMAX, 10.0f, false).value, 0.1f,
              TOLERANCE);

  const float logits[3] = {1.0f, 2.0f, 3.0f};
  EXPECT_NEAR(process_output(logits, 3, OutputProcessing::OUTPUT_PROCESSING_SOFTMAX, 10.0f, false).value, 0.2f,
              TOLERANCE);
  EXPECT_NEAR(process_output(logits, 3, OutputProcessing::OUTPUT_PROCESSING_QAT_QUANTIZED, 10.0f, false).value, 0.2f,
              TOLERANCE);
}

TEST(OutputProcessor, NonPositiveScaleFactorIsIgnored) {
  const float output[3] = {0.1f, 2.5f, -0.4f};
  EXPECT_FLOAT_EQ(process_output(output, 3, OutputProcessing::OUTPUT_PROCESSING_ARGMAX, 0.0f, false).value, 1.0f);
  EXPECT_FLOAT_EQ(process_output(output, 3, OutputProcessing::OUTPUT_PROCESSING_ARGMAX, -2.0f, false).value, 1.0f);
}

TEST(OutputProcessor, SoftmaxConfidenceIsTheWinningProbability) {
  const float logits[3] = {1.0f, 2.0f, 3.0f};
  const ProcessedOutput result = process_output(logits, 3, OutputProcessing::OUTPUT_PROCESSING_SOFTMAX, 1.0f, false);
  EXPECT_FLOAT_EQ(result.value, 2.0f);
  EXPECT_NEAR(result.confidence, 0.6652409558f, TOLERANCE);
}

// Regression: the confidence used to be (max - min) / (max - min), which is always 1.0.
TEST(OutputProcessor, QatQuantizedConfidenceIsTheWinningProbability) {
  const float probabilities[3] = {0.05f, 0.9f, 0.05f};
  const ProcessedOutput result =
      process_output(probabilities, 3, OutputProcessing::OUTPUT_PROCESSING_QAT_QUANTIZED, 1.0f, false);
  EXPECT_FLOAT_EQ(result.value, 1.0f);
  EXPECT_FLOAT_EQ(result.confidence, 0.9f);

  const float overshoot[3] = {0.2f, 1.5f, 0.1f};
  const ProcessedOutput clamped =
      process_output(overshoot, 3, OutputProcessing::OUTPUT_PROCESSING_QAT_QUANTIZED, 1.0f, false);
  EXPECT_FLOAT_EQ(clamped.confidence, 1.0f);
}

TEST(OutputProcessor, AutoDetectRecognizesProbabilities) {
  const float probabilities[3] = {0.05f, 0.9f, 0.05f};
  const ProcessedOutput result =
      process_output(probabilities, 3, OutputProcessing::OUTPUT_PROCESSING_AUTO_DETECT, 1.0f, false);
  EXPECT_FLOAT_EQ(result.value, 1.0f);
  EXPECT_FLOAT_EQ(result.confidence, 0.9f);
}

// An exact one-hot vector sums to 1, so the probability check wins and the one-hot branch is not used.
TEST(OutputProcessor, AutoDetectExactOneHotIsTreatedAsProbabilities) {
  const float one_hot[3] = {0.0f, 1.0f, 0.0f};
  const ProcessedOutput result =
      process_output(one_hot, 3, OutputProcessing::OUTPUT_PROCESSING_AUTO_DETECT, 1.0f, false);
  EXPECT_FLOAT_EQ(result.value, 1.0f);
  EXPECT_FLOAT_EQ(result.confidence, 1.0f);
}

// Regression: the one-hot branch used to report 1.0 whatever the single non-zero value was.
TEST(OutputProcessor, AutoDetectSingleNonZeroKeepsItsValueAsTheConfidence) {
  const float one_hot[3] = {0.0f, 0.7f, 0.0f};
  const ProcessedOutput result =
      process_output(one_hot, 3, OutputProcessing::OUTPUT_PROCESSING_AUTO_DETECT, 1.0f, false);
  EXPECT_FLOAT_EQ(result.value, 1.0f);
  EXPECT_FLOAT_EQ(result.confidence, 0.7f);
}

TEST(OutputProcessor, AutoDetectLogits) {
  const float logits[3] = {1.0f, 2.0f, 3.0f};
  const ProcessedOutput result =
      process_output(logits, 3, OutputProcessing::OUTPUT_PROCESSING_AUTO_DETECT, 1.0f, false);
  EXPECT_FLOAT_EQ(result.value, 2.0f);
  EXPECT_NEAR(result.confidence, 0.6652409558f, TOLERANCE);
}

TEST(OutputProcessor, InvalidArgumentsReturnZero) {
  const float output[1] = {1.0f};
  const ProcessedOutput null_data = process_output(nullptr, 1, OutputProcessing::OUTPUT_PROCESSING_ARGMAX, 1.0f, false);
  EXPECT_FLOAT_EQ(null_data.value, 0.0f);
  EXPECT_FLOAT_EQ(null_data.confidence, 0.0f);

  const ProcessedOutput no_classes = process_output(output, 0, OutputProcessing::OUTPUT_PROCESSING_ARGMAX, 1.0f, false);
  EXPECT_FLOAT_EQ(no_classes.value, 0.0f);
  EXPECT_FLOAT_EQ(no_classes.confidence, 0.0f);
}

}  // namespace esphome::tflite_output_processor::testing
