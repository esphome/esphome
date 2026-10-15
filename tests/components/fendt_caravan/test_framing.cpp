#include "esphome/components/fendt_caravan/decoders.h"
#include "esphome/components/fendt_caravan/framing.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>

namespace esphome::fendt_caravan::testing {

static std::string chunk_at(const char *command, size_t pos) {
  char out[CHUNK_SIZE + 1];
  const size_t n = next_chunk(command, strlen(command), pos, out);
  return std::string(out, n);
}

TEST(NextChunk, ShortCommandIsOneChunk) { EXPECT_EQ(chunk_at("net-BT_VARS", 0), "net-BT_VARS"); }

TEST(NextChunk, LongCommandContinuesWithMarkerUntilTheLastChunk) {
  const char *command = "net-BT_ID-c0:ee:fb:90:b0:a7";  // 27 bytes
  EXPECT_EQ(chunk_at(command, 0), "net-BT_ID-c0:ee:f@");
  EXPECT_EQ(chunk_at(command, CHUNK_SIZE), "b:90:b0:a7");
}

TEST(NextChunk, ExactMultipleHasNoEmptyTail) {
  std::string command(2 * CHUNK_SIZE, 'x');
  EXPECT_EQ(chunk_at(command.c_str(), CHUNK_SIZE), std::string(CHUNK_SIZE, 'x'));
}

template<size_t N> static FrameResult push(FrameAssembler<N> &rx, const char *fragment) {
  return rx.push(reinterpret_cast<const uint8_t *>(fragment), strlen(fragment));
}

TEST(FrameAssembler, SingleFragmentIsComplete) {
  FrameAssembler<32> rx;
  EXPECT_EQ(push(rx, "TEMP_IN:21,5"), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(rx.data(), "TEMP_IN:21,5");
  EXPECT_TRUE(rx.pending());
  rx.reset();
  EXPECT_FALSE(rx.pending());
}

TEST(FrameAssembler, ThreeFragmentsJoinWithoutTheMarkers) {
  FrameAssembler<32> rx;
  EXPECT_EQ(push(rx, "SOFTWARE_VER@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(push(rx, "SION:1.2@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(push(rx, ".3"), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(rx.data(), "SOFTWARE_VERSION:1.2.3");
}

TEST(FrameAssembler, EmptyFragmentCompletesWhatIsPending) {
  FrameAssembler<32> rx;
  EXPECT_EQ(push(rx, "A:1@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(push(rx, ""), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(rx.data(), "A:1");
}

TEST(FrameAssembler, OverflowDropsEverythingSoFar) {
  FrameAssembler<16> rx;
  EXPECT_EQ(push(rx, "0123456789@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(push(rx, "0123456789"), FrameResult::FRAME_RESULT_OVERFLOW);
  EXPECT_FALSE(rx.pending());
  EXPECT_EQ(push(rx, "B:2"), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(rx.data(), "B:2");
}

TEST(FrameAssembler, OverflowDropsTheRestOfThatReport) {
  FrameAssembler<16> rx;
  EXPECT_EQ(push(rx, "0123456789@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(push(rx, "0123456789@"), FrameResult::FRAME_RESULT_OVERFLOW);
  EXPECT_TRUE(rx.pending());  // the tail is still arriving, so the timeout can clear a stuck drop
  EXPECT_EQ(push(rx, "tail@"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_EQ(push(rx, "end"), FrameResult::FRAME_RESULT_INCOMPLETE);
  EXPECT_FALSE(rx.pending());
  EXPECT_EQ(push(rx, "B:2"), FrameResult::FRAME_RESULT_COMPLETE);
  EXPECT_STREQ(rx.data(), "B:2");
}

TEST(Decoders, FloatTakesCommaPaddingAndUnit) {
  float value = 0;
  char temperature[] = "21,5^C";
  EXPECT_TRUE(parse_panel_float(temperature, value));
  EXPECT_FLOAT_EQ(value, 21.5f);
  char voltage[] = " 13,9 V";
  EXPECT_TRUE(parse_panel_float(voltage, value));
  EXPECT_FLOAT_EQ(value, 13.9f);
  char negative[] = "-3,0";
  EXPECT_TRUE(parse_panel_float(negative, value));
  EXPECT_FLOAT_EQ(value, -3.0f);
}

TEST(Decoders, FloatWithoutDigitsFails) {
  float value = 0;
  char empty[] = "";
  EXPECT_FALSE(parse_panel_float(empty, value));
  char text[] = "^C";
  EXPECT_FALSE(parse_panel_float(text, value));
}

TEST(Decoders, IntTakesPadding) {
  int value = 0;
  EXPECT_TRUE(parse_panel_int("  0", value));
  EXPECT_EQ(value, 0);
  EXPECT_TRUE(parse_panel_int("2", value));
  EXPECT_EQ(value, 2);
  EXPECT_FALSE(parse_panel_int("", value));
  EXPECT_FALSE(parse_panel_int("x", value));
}

static int bool_of(const char *text) {
  bool value = false;
  return parse_panel_bool(text, value) ? value : -1;
}

TEST(Decoders, BoolSpellings) {
  EXPECT_EQ(bool_of("1"), 1);
  EXPECT_EQ(bool_of("01"), 1);
  EXPECT_EQ(bool_of("ON"), 1);
  EXPECT_EQ(bool_of("true"), 1);
  EXPECT_EQ(bool_of(" 1"), 1);
  EXPECT_EQ(bool_of("0"), 0);
  EXPECT_EQ(bool_of("00"), 0);
  EXPECT_EQ(bool_of("off"), 0);
  EXPECT_EQ(bool_of("False"), 0);
  EXPECT_EQ(bool_of(" 1\r"), 1);
  EXPECT_EQ(bool_of("off \n"), 0);
}

TEST(Decoders, BoolRejectsUnknownText) {
  EXPECT_EQ(bool_of(""), -1);
  EXPECT_EQ(bool_of("yes"), -1);
  EXPECT_EQ(bool_of(" \r\n"), -1);
  EXPECT_EQ(bool_of("1 V"), -1);
}

}  // namespace esphome::fendt_caravan::testing
