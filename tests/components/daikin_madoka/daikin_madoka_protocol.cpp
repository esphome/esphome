#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <vector>

#include "esphome/components/daikin_madoka/daikin_madoka_protocol.h"

namespace esphome::daikin_madoka::testing {

// Builds a message {len, 0, cmd_hi, cmd_lo, args...} with the leading length byte filled in.
static std::vector<uint8_t> make_message(uint16_t cmd, const std::vector<uint8_t> &args) {
  std::vector<uint8_t> msg{0, 0, static_cast<uint8_t>(cmd >> 8), static_cast<uint8_t>(cmd & 0xFF)};
  msg.insert(msg.end(), args.begin(), args.end());
  msg[0] = static_cast<uint8_t>(msg.size());
  return msg;
}

// Splits a message into notifications of a chunk id byte plus up to 19 payload bytes.
static std::vector<std::vector<uint8_t>> make_chunks(const std::vector<uint8_t> &msg) {
  std::vector<std::vector<uint8_t>> chunks;
  for (size_t pos = 0, id = 0; pos < msg.size(); pos += MAX_CHUNK_SIZE - 1, id++) {
    std::vector<uint8_t> chunk{static_cast<uint8_t>(id)};
    size_t end = std::min(msg.size(), pos + MAX_CHUNK_SIZE - 1);
    chunk.insert(chunk.end(), msg.begin() + pos, msg.begin() + end);
    chunks.push_back(chunk);
  }
  return chunks;
}

// A setpoint reply padded with extra arguments, like the 44 argument replies seen from real controllers.
static std::vector<uint8_t> make_wide_setpoint_message() {
  std::vector<uint8_t> args;
  for (uint8_t id = 0x30; id < 0x30 + 42; id++) {
    args.push_back(id);
    args.push_back(1);
    args.push_back(id);
  }
  args.insert(args.end(), {0x20, 0x02, 0x0B, 0x00, 0x21, 0x02, 0x0A, 0x80});
  return make_message(0x0040, args);
}

TEST(DaikinMadokaAssembler, SingleChunkMessage) {
  MessageAssembler assembler;
  auto msg = make_message(0x0020, {0x20, 0x01, 0x01});
  auto chunks = make_chunks(msg);
  ASSERT_EQ(chunks.size(), 1u);
  EXPECT_EQ(assembler.feed(chunks[0]), ChunkResult::CHUNK_RESULT_COMPLETE);
  auto out = assembler.message();
  EXPECT_EQ(std::vector<uint8_t>(out.begin(), out.end()), msg);
}

TEST(DaikinMadokaAssembler, MultiChunkMessage) {
  MessageAssembler assembler;
  auto msg = make_wide_setpoint_message();
  auto chunks = make_chunks(msg);
  ASSERT_GT(chunks.size(), 2u);
  for (size_t i = 0; i + 1 < chunks.size(); i++) {
    EXPECT_EQ(assembler.feed(chunks[i]), ChunkResult::CHUNK_RESULT_PARTIAL) << "chunk " << i;
  }
  EXPECT_EQ(assembler.feed(chunks.back()), ChunkResult::CHUNK_RESULT_COMPLETE);
  auto out = assembler.message();
  EXPECT_EQ(std::vector<uint8_t>(out.begin(), out.end()), msg);

  // The next chunk 0 starts a fresh message
  auto next = make_message(0x0020, {0x20, 0x01, 0x00});
  EXPECT_EQ(assembler.feed(make_chunks(next)[0]), ChunkResult::CHUNK_RESULT_COMPLETE);
  out = assembler.message();
  EXPECT_EQ(std::vector<uint8_t>(out.begin(), out.end()), next);
}

TEST(DaikinMadokaAssembler, OutOfOrderChunkDropsMessage) {
  MessageAssembler assembler;
  auto chunks = make_chunks(make_wide_setpoint_message());
  ASSERT_GT(chunks.size(), 2u);
  EXPECT_EQ(assembler.feed(chunks[0]), ChunkResult::CHUNK_RESULT_PARTIAL);
  EXPECT_EQ(assembler.feed(chunks[2]), ChunkResult::CHUNK_RESULT_OUT_OF_ORDER);
  EXPECT_EQ(assembler.expected_chunk_id(), 0);
  EXPECT_TRUE(assembler.message().empty());
  // The rest of the dropped message is discarded too
  EXPECT_EQ(assembler.feed(chunks[1]), ChunkResult::CHUNK_RESULT_OUT_OF_ORDER);

  // A new chunk 0 in the middle of a message restarts reassembly
  EXPECT_EQ(assembler.feed(chunks[0]), ChunkResult::CHUNK_RESULT_PARTIAL);
  auto msg = make_message(0x0020, {0x20, 0x01, 0x01});
  EXPECT_EQ(assembler.feed(make_chunks(msg)[0]), ChunkResult::CHUNK_RESULT_COMPLETE);
}

TEST(DaikinMadokaAssembler, RejectsShortAndOverlongChunks) {
  MessageAssembler assembler;
  const std::vector<uint8_t> id_only{0x00};
  EXPECT_EQ(assembler.feed(id_only), ChunkResult::CHUNK_RESULT_INVALID_LENGTH);
  // Length byte says 3 but the chunk carries 5 bytes
  const std::vector<uint8_t> overlong{0x00, 0x03, 0x00, 0x00, 0x20, 0x00};
  EXPECT_EQ(assembler.feed(overlong), ChunkResult::CHUNK_RESULT_TOO_LONG);
  EXPECT_TRUE(assembler.message().empty());
}

TEST(DaikinMadokaFindArg, FindsArgumentsInWideMessage) {
  auto msg = make_wide_setpoint_message();
  ASSERT_TRUE(is_valid_message(msg));
  EXPECT_EQ(message_function_id(msg), 0x0040);
  auto high = find_arg(msg, 0x20);
  ASSERT_EQ(high.size(), 2u);
  EXPECT_FLOAT_EQ(decode_setpoint(high), 22.0f);
  auto low = find_arg(msg, 0x21);
  ASSERT_EQ(low.size(), 2u);
  EXPECT_FLOAT_EQ(decode_setpoint(low), 21.0f);
  auto padding = find_arg(msg, 0x30 + 41);
  ASSERT_EQ(padding.size(), 1u);
  EXPECT_EQ(padding[0], 0x30 + 41);
  EXPECT_TRUE(find_arg(msg, 0x7F).empty());
}

TEST(DaikinMadokaFindArg, TruncatedArgumentIsNotReturned) {
  // Argument 0x21 claims 4 bytes but only 2 remain
  auto msg = make_message(0x0040, {0x20, 0x01, 0x05, 0x21, 0x04, 0x01, 0x02});
  EXPECT_EQ(find_arg(msg, 0x20).size(), 1u);
  EXPECT_TRUE(find_arg(msg, 0x21).empty());
  // A dangling id byte with no length byte is ignored
  auto dangling = make_message(0x0040, {0x20, 0x01, 0x05, 0x21});
  EXPECT_TRUE(find_arg(dangling, 0x21).empty());
}

TEST(DaikinMadokaFindArg, MessageValidation) {
  const std::vector<uint8_t> too_short{0x03, 0x00, 0x00};
  EXPECT_FALSE(is_valid_message(too_short));
  auto msg = make_message(0x0020, {0x20, 0x01, 0x01});
  msg[0]++;
  EXPECT_FALSE(is_valid_message(msg));
}

static std::span<const uint8_t> as_span(const QueryArgs &args) { return {args.begin(), args.size()}; }

TEST(DaikinMadokaMergeArgs, ReplacesAndAppendsRecords) {
  // A call that only sets the low setpoint joins a queued high one, and a newer high replaces it
  QueryArgs args;
  append_setpoint(args, 0x20, 24.0f);
  QueryArgs low;
  append_setpoint(low, 0x21, 20.0f);
  EXPECT_TRUE(merge_args(args, as_span(low)));
  EXPECT_EQ(args.size(), 8u);
  QueryArgs high;
  append_setpoint(high, 0x20, 26.0f);
  EXPECT_TRUE(merge_args(args, as_span(high)));
  EXPECT_EQ(args.size(), 8u);
  EXPECT_FLOAT_EQ(26.0f, decode_setpoint(as_span(args).subspan(2, 2)));
  EXPECT_FLOAT_EQ(20.0f, decode_setpoint(as_span(args).subspan(6, 2)));
}

TEST(DaikinMadokaMergeArgs, LeavesArgsAloneWhenARecordDoesNotFit) {
  QueryArgs args;
  append_setpoint(args, 0x20, 24.0f);
  append_setpoint(args, 0x21, 20.0f);
  const std::array<uint8_t, 3> extra{0x22, 0x01, 0x00};
  EXPECT_FALSE(merge_args(args, extra));
  EXPECT_EQ(args.size(), 8u);
  // A record of another length for the same id is not merged either
  const std::array<uint8_t, 3> short_high{0x20, 0x01, 0x00};
  EXPECT_FALSE(merge_args(args, short_high));
  EXPECT_EQ(args[1], 2u);
}

TEST(DaikinMadokaSetpoint, RoundTrip) {
  StaticVector<uint8_t, MAX_QUERY_ARGS> args;
  append_setpoint(args, 0x20, 22.5f);
  append_setpoint(args, 0x21, 40.0f);  // clamped to MAX_TEMP
  ASSERT_EQ(args.size(), 8u);
  EXPECT_EQ(args[0], 0x20);
  EXPECT_EQ(args[1], 0x02);
  EXPECT_EQ(args[2], 0x0B);
  EXPECT_EQ(args[3], 0x40);
  EXPECT_FLOAT_EQ(decode_setpoint(std::span<const uint8_t>(args.data() + 2, 2)), 22.5f);
  EXPECT_EQ(args[4], 0x21);
  EXPECT_FLOAT_EQ(decode_setpoint(std::span<const uint8_t>(args.data() + 6, 2)), MAX_TEMP);

  StaticVector<uint8_t, MAX_QUERY_ARGS> low;
  append_setpoint(low, 0x21, 5.0f);  // clamped to MIN_TEMP
  EXPECT_FLOAT_EQ(decode_setpoint(std::span<const uint8_t>(low.data() + 2, 2)), MIN_TEMP);
}

}  // namespace esphome::daikin_madoka::testing
