#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

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

std::vector<Detection> select(const std::vector<Detection> &candidates, size_t cap) {
  std::vector<Detection> kept(cap);
  kept.resize(select_detections(kept.data(), cap, 0.5f, candidates.size(), [&](size_t index, Detection &detection) {
    detection = candidates[index];
    return true;
  }));
  return kept;
}
}  // namespace

// An overlapping pair must not fill the cap and hide a distinct detection.
TEST(DetectionDecoder, CapAppliesAfterSuppression) {
  const auto kept = select({full_frame(0.9f), full_frame(0.8f), small_box(0.7f)}, 2);

  ASSERT_EQ(kept.size(), 2u);
  EXPECT_FLOAT_EQ(kept[0].score, 0.9f);
  EXPECT_FLOAT_EQ(kept[1].score, 0.7f);
}

// A suppressed box suppresses nothing, whatever order the candidates arrive in.
TEST(DetectionDecoder, SuppressedBoxDoesNotSuppress) {
  const auto kept = select({make_box(0.8f, 0, 0.0f, 0.0f, 0.7f, 1.0f), make_box(0.7f, 0, 0.0f, 0.0f, 0.45f, 1.0f),
                            make_box(0.9f, 0, 0.0f, 0.0f, 1.0f, 1.0f)},
                           3);

  ASSERT_EQ(kept.size(), 2u);
  EXPECT_FLOAT_EQ(kept[0].score, 0.9f);
  EXPECT_FLOAT_EQ(kept[1].score, 0.7f);
}

// Equal scores are each considered once.
TEST(DetectionDecoder, EqualScoresAreNotRepeated) {
  const auto kept = select({make_box(0.9f, 0, 0.0f, 0.0f, 0.2f, 0.2f), make_box(0.9f, 0, 0.3f, 0.3f, 0.5f, 0.5f)}, 4);

  ASSERT_EQ(kept.size(), 2u);
  EXPECT_FLOAT_EQ(kept[0].score, 0.9f);
  EXPECT_FLOAT_EQ(kept[1].score, 0.9f);
}

// Distinct boxes are capped to the best ones.
TEST(DetectionDecoder, CapKeepsTheBestBoxes) {
  const auto kept = select({make_box(0.7f, 0, 0.6f, 0.6f, 0.8f, 0.8f), make_box(0.9f, 0, 0.0f, 0.0f, 0.2f, 0.2f),
                            make_box(0.8f, 0, 0.3f, 0.3f, 0.5f, 0.5f)},
                           2);

  ASSERT_EQ(kept.size(), 2u);
  EXPECT_FLOAT_EQ(kept[0].score, 0.9f);
  EXPECT_FLOAT_EQ(kept[1].score, 0.8f);
}

// Overlap only suppresses boxes of the same class.
TEST(DetectionDecoder, DifferentClassesAreNotSuppressed) {
  const auto kept = select({full_frame(0.9f, 0), full_frame(0.8f, 1)}, 2);

  ASSERT_EQ(kept.size(), 2u);
  EXPECT_FLOAT_EQ(kept[0].score, 0.9f);
  EXPECT_FLOAT_EQ(kept[1].score, 0.8f);
}

TEST(YoloCandidateTest, TheScoreIsObjectnessTimesTheBestClass) {
  const std::vector<float> fields{0.5f, 0.4f, 0.2f, 0.4f, 0.8f, 0.1f, 0.9f, 0.3f};
  Detection detection;
  ASSERT_TRUE(yolo_candidate([&](size_t field) { return fields[field]; }, 3, true, 0.5f, detection));
  EXPECT_EQ(detection.class_id, 1);
  EXPECT_FLOAT_EQ(detection.score, 0.72f);
  EXPECT_FLOAT_EQ(detection.x_min, 0.4f);
  EXPECT_FLOAT_EQ(detection.x_max, 0.6f);
  EXPECT_FLOAT_EQ(detection.y_min, 0.2f);
  EXPECT_FLOAT_EQ(detection.y_max, 0.6f);
}

TEST(YoloCandidateTest, WithoutObjectnessTheClassesFollowTheBox) {
  const std::vector<float> fields{0.5f, 0.5f, 1.0f, 1.0f, 0.2f, 0.7f};
  Detection detection;
  ASSERT_TRUE(yolo_candidate([&](size_t field) { return fields[field]; }, 2, false, 0.5f, detection));
  EXPECT_EQ(detection.class_id, 1);
  EXPECT_FLOAT_EQ(detection.score, 0.7f);
}

TEST(YoloCandidateTest, ALowScoreIsSkipped) {
  const std::vector<float> fields{0.5f, 0.5f, 1.0f, 1.0f, 0.6f, 0.6f};
  Detection detection;
  EXPECT_FALSE(yolo_candidate([&](size_t field) { return fields[field]; }, 1, true, 0.5f, detection));
}

TEST(YoloCandidateTest, ALowObjectnessSkipsTheClassScores) {
  size_t highest_field_read = 0;
  Detection detection;
  EXPECT_FALSE(yolo_candidate(
      [&](size_t field) {
        highest_field_read = std::max(highest_field_read, field);
        return 0.1f;
      },
      80, true, 0.5f, detection));
  EXPECT_EQ(highest_field_read, 4u);
}

namespace {
/// Two candidates with two classes, each as {x, y, width, height, (objectness,) class scores} in quarters.
std::vector<Detection> decode_quantized(DetectionFormat format, const std::vector<std::vector<int>> &candidates) {
  const size_t fields = candidates[0].size();
  const bool v5 = format == DetectionFormat::DETECTION_FORMAT_YOLOV5;
  // An int8 tensor in the model's layout, with scale 1/4 and zero point -2.
  std::vector<int8_t> tensor(fields * candidates.size());
  for (size_t c = 0; c < candidates.size(); c++) {
    for (size_t f = 0; f < fields; f++)
      tensor[v5 ? c * fields + f : f * candidates.size() + c] = static_cast<int8_t>(candidates[c][f] - 2);
  }
  std::vector<Detection> kept(4);
  kept.resize(decode_yolo(kept.data(), kept.size(), 0.5f, 0.5f, format, fields, candidates.size(),
                          [&](size_t index) { return (tensor[index] + 2) * 0.25f; }));
  return kept;
}

void expect_two_boxes(const std::vector<Detection> &kept) {
  ASSERT_EQ(kept.size(), 2u);
  // The better score comes first: the second candidate, class 1, centered at (2, 2) with size 1x1.
  EXPECT_EQ(kept[0].class_id, 1);
  EXPECT_FLOAT_EQ(kept[0].score, 1.0f);
  EXPECT_FLOAT_EQ(kept[0].x_min, 1.5f);
  EXPECT_FLOAT_EQ(kept[0].y_max, 2.5f);
  EXPECT_EQ(kept[1].class_id, 0);
  EXPECT_FLOAT_EQ(kept[1].score, 0.75f);
  EXPECT_FLOAT_EQ(kept[1].x_max, 0.75f);
  EXPECT_FLOAT_EQ(kept[1].y_min, 0.0f);
}
}  // namespace

TEST(YoloLayoutTest, Yolov5ReadsOneCandidatePerRow) {
  expect_two_boxes(
      decode_quantized(DetectionFormat::DETECTION_FORMAT_YOLOV5, {{2, 2, 2, 4, 4, 3, 1}, {8, 8, 4, 4, 4, 1, 4}}));
}

TEST(YoloLayoutTest, Yolov8ReadsOneCandidatePerColumn) {
  expect_two_boxes(
      decode_quantized(DetectionFormat::DETECTION_FORMAT_YOLOV8, {{2, 2, 2, 4, 3, 1}, {8, 8, 4, 4, 1, 4}}));
}

}  // namespace esphome::tflite_detection::testing
