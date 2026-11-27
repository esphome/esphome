#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/tflite_ctc/tflite_ctc.h"

namespace esphome::tflite_ctc::testing {

namespace {
constexpr size_t CLASSES = 3;

/// Decode a matrix whose best class at each step is the given one.
std::vector<uint16_t> decode(const std::vector<size_t> &best, size_t blank) {
  std::vector<uint16_t> ids(best.size());
  ids.resize(ctc_greedy_decode(ids.data(), best.size(), CLASSES, blank,
                               [&](size_t index) { return index % CLASSES == best[index / CLASSES] ? 1.0f : -1.0f; }));
  return ids;
}
}  // namespace

TEST(CtcDecoderTest, RepeatsCollapseAndBlanksAreDropped) {
  EXPECT_EQ(decode({2, 0, 0, 2, 1, 1, 2}, 2), (std::vector<uint16_t>{0, 1}));
}

TEST(CtcDecoderTest, ABlankSeparatesARepeatedClass) { EXPECT_EQ(decode({0, 2, 0}, 2), (std::vector<uint16_t>{0, 0})); }

TEST(CtcDecoderTest, TheBlankMayBeTheFirstClass) {
  EXPECT_EQ(decode({0, 1, 0, 2, 2}, 0), (std::vector<uint16_t>{1, 2}));
}

TEST(CtcDecoderTest, OnlyBlanksDecodeToNothing) { EXPECT_TRUE(decode({2, 2, 2}, 2).empty()); }

TEST(CtcDecoderTest, EveryStepCanProduceAnId) {
  EXPECT_EQ(decode({0, 1, 0, 1}, 2), (std::vector<uint16_t>{0, 1, 0, 1}));
}

TEST(CtcDecoderTest, ALabelListLeavesOutTheBlank) {
  EXPECT_EQ(ctc_label_index(0, 2), 0u);
  EXPECT_EQ(ctc_label_index(1, 0), 0u);
  EXPECT_EQ(ctc_label_index(2, 0), 1u);
  EXPECT_EQ(ctc_label_index(0, 1), 0u);
  EXPECT_EQ(ctc_label_index(2, 1), 1u);
}

}  // namespace esphome::tflite_ctc::testing
