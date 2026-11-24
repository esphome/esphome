#include <gtest/gtest.h>

#include "esphome/components/tflite_detection/input_detection.h"

namespace esphome::tflite_detection::testing {

namespace {
Detection make_box(float score, int class_id, float x_min, float y_min, float x_max, float y_max) {
  Detection detection;
  detection.score = score;
  detection.class_id = class_id;
  detection.x_min = x_min;
  detection.y_min = y_min;
  detection.x_max = x_max;
  detection.y_max = y_max;
  return detection;
}

Detection full_frame(float score, int class_id = 0) { return make_box(score, class_id, 0.0f, 0.0f, 1.0f, 1.0f); }

Detection small_box(float score, int class_id = 0) { return make_box(score, class_id, 0.2f, 0.2f, 0.4f, 0.4f); }
}  // namespace

// Regression: the cap used to be applied while candidates were collected, so an
// overlapping pair could fill the buffer and hide a distinct detection.
TEST(DetectionDecoder, CapAppliesAfterSuppression) {
  Detection detections[2];
  size_t count = 0;
  count = insert_detection(detections, count, 2, 0.5f, full_frame(0.9f));
  count = insert_detection(detections, count, 2, 0.5f, full_frame(0.8f));
  count = insert_detection(detections, count, 2, 0.5f, small_box(0.7f));

  ASSERT_EQ(count, 2u);
  EXPECT_FLOAT_EQ(detections[0].score, 0.9f);
  EXPECT_FLOAT_EQ(detections[1].score, 0.7f);
}

// A later, higher-scored box replaces the lower-scored box it overlaps.
TEST(DetectionDecoder, HigherScoredBoxReplacesTheOneItSuppresses) {
  Detection detections[2];
  size_t count = 0;
  count = insert_detection(detections, count, 2, 0.5f, full_frame(0.8f));
  count = insert_detection(detections, count, 2, 0.5f, full_frame(0.9f));

  ASSERT_EQ(count, 1u);
  EXPECT_FLOAT_EQ(detections[0].score, 0.9f);
}

// Distinct boxes are still capped once they survive suppression.
TEST(DetectionDecoder, CapStillLimitsDistinctBoxes) {
  Detection detections[2];
  size_t count = 0;
  count = insert_detection(detections, count, 2, 0.5f, make_box(0.9f, 0, 0.0f, 0.0f, 0.2f, 0.2f));
  count = insert_detection(detections, count, 2, 0.5f, make_box(0.8f, 0, 0.3f, 0.3f, 0.5f, 0.5f));
  count = insert_detection(detections, count, 2, 0.5f, make_box(0.7f, 0, 0.6f, 0.6f, 0.8f, 0.8f));

  ASSERT_EQ(count, 2u);
  EXPECT_FLOAT_EQ(detections[0].score, 0.9f);
  EXPECT_FLOAT_EQ(detections[1].score, 0.8f);
}

// Overlap only suppresses boxes of the same class.
TEST(DetectionDecoder, DifferentClassesAreNotSuppressed) {
  Detection detections[2];
  size_t count = 0;
  count = insert_detection(detections, count, 2, 0.5f, full_frame(0.9f, 0));
  count = insert_detection(detections, count, 2, 0.5f, full_frame(0.8f, 1));

  ASSERT_EQ(count, 2u);
}

}  // namespace esphome::tflite_detection::testing
