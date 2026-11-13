#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/fendt_caravan/framing.h"

namespace esphome::fendt_caravan::testing {

// Splits a command the way the component writes it
static std::vector<std::string> chunks(const std::string &command) {
  std::vector<std::string> result;
  char out[CHUNK_SIZE + 1];
  size_t pos = 0;
  while (pos < command.size()) {
    const size_t n = next_chunk(command.data(), command.size(), pos, out);
    result.emplace_back(out, n);
    pos += std::min(CHUNK_SIZE, command.size() - pos);
  }
  return result;
}

TEST(FendtNextChunk, ShortCommandIsOneChunk) { EXPECT_EQ(chunks("LIGHT:1"), std::vector<std::string>{"LIGHT:1"}); }

TEST(FendtNextChunk, ExactChunkSizeHasNoContinuation) {
  const std::string command(CHUNK_SIZE, 'A');
  EXPECT_EQ(chunks(command), std::vector<std::string>{command});
}

TEST(FendtNextChunk, LongerCommandContinues) {
  const std::string command = std::string(CHUNK_SIZE, 'A') + "BC";
  EXPECT_EQ(chunks(command), (std::vector<std::string>{std::string(CHUNK_SIZE, 'A') + "@", "BC"}));
}

TEST(FendtNextChunk, TwiceChunkSizeEndsWithoutContinuation) {
  const std::string command = std::string(CHUNK_SIZE, 'A') + std::string(CHUNK_SIZE, 'B');
  EXPECT_EQ(chunks(command),
            (std::vector<std::string>{std::string(CHUNK_SIZE, 'A') + "@", std::string(CHUNK_SIZE, 'B')}));
}

class FrameAssemblerTest : public ::testing::Test {
 protected:
  FrameResult push_(const std::string &fragment) {
    return this->assembler_.push(reinterpret_cast<const uint8_t *>(fragment.data()), fragment.size());
  }

  // Holds at most 7 characters plus the NUL
  FrameAssembler<8> assembler_;
};

TEST_F(FrameAssemblerTest, SingleFragment) {
  EXPECT_FALSE(this->assembler_.pending());
  EXPECT_EQ(this->push_("A:1"), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(this->assembler_.data(), "A:1");
  EXPECT_TRUE(this->assembler_.pending());
  this->assembler_.reset();
  EXPECT_FALSE(this->assembler_.pending());
}

TEST_F(FrameAssemblerTest, JoinsContinuedFragments) {
  EXPECT_EQ(this->push_("A:@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_TRUE(this->assembler_.pending());
  EXPECT_EQ(this->push_("12@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(this->push_("3"), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(this->assembler_.data(), "A:123");
}

TEST_F(FrameAssemblerTest, ExactCapacityFits) {
  EXPECT_EQ(this->push_("ABCD@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(this->push_("EFG"), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(this->assembler_.data(), "ABCDEFG");
}

TEST_F(FrameAssemblerTest, OverflowOfLastFragmentRecovers) {
  EXPECT_EQ(this->push_("ABCD@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(this->push_("EFGH"), FrameResult::FRAME_RESULT_OVERFLOW);
  EXPECT_FALSE(this->assembler_.pending());
  EXPECT_EQ(this->push_("B:2"), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(this->assembler_.data(), "B:2");
}

TEST_F(FrameAssemblerTest, DropsRestOfOverflowedReport) {
  EXPECT_EQ(this->push_("ABCDEFGH@"), FrameResult::FRAME_RESULT_OVERFLOW);
  EXPECT_TRUE(this->assembler_.pending());
  EXPECT_EQ(this->push_("IJ@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  // The last piece of the dropped report must not come out as a report of its own
  EXPECT_EQ(this->push_("K:1"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_FALSE(this->assembler_.pending());
  EXPECT_EQ(this->push_("B:2"), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(this->assembler_.data(), "B:2");
}

TEST_F(FrameAssemblerTest, EmptyFragmentIsEmptyReport) {
  EXPECT_EQ(this->assembler_.push(nullptr, 0), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(this->assembler_.data(), "");
}

TEST_F(FrameAssemblerTest, EmptyFragmentEndsContinuedReport) {
  EXPECT_EQ(this->push_("A:1@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(this->assembler_.push(nullptr, 0), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(this->assembler_.data(), "A:1");
}

TEST_F(FrameAssemblerTest, EmptyFragmentEndsDroppedReport) {
  EXPECT_EQ(this->push_("ABCDEFGH@"), FrameResult::FRAME_RESULT_OVERFLOW);
  EXPECT_EQ(this->assembler_.push(nullptr, 0), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_FALSE(this->assembler_.pending());
}

TEST_F(FrameAssemblerTest, LoneContinuationMarker) {
  EXPECT_EQ(this->push_("@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(this->push_("A:1"), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(this->assembler_.data(), "A:1");
}

}  // namespace esphome::fendt_caravan::testing
