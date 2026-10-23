#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/display/init_sequence.h"

namespace esphome::display::testing {

TEST(InitSequenceReader, WalksCommandsAndDelays) {
  static const uint8_t SEQ[] = {0x11, 0x00, 120, 0xFF, 0x3A, 0x01, 0x55, 0x2A, 0x02, 0x00, 0xEF};
  InitSequenceReader reader(SEQ, sizeof(SEQ));
  InitSequenceEntry entry{};

  ASSERT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_COMMAND);
  EXPECT_EQ(entry.value, 0x11);
  EXPECT_EQ(entry.count, 0);

  ASSERT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_DELAY);
  EXPECT_EQ(entry.value, 120);

  ASSERT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_COMMAND);
  EXPECT_EQ(entry.value, 0x3A);
  ASSERT_EQ(entry.count, 1);
  EXPECT_EQ(entry.args, &SEQ[6]);

  ASSERT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_COMMAND);
  EXPECT_EQ(entry.value, 0x2A);
  ASSERT_EQ(entry.count, 2);
  EXPECT_EQ(entry.args[0], 0x00);
  EXPECT_EQ(entry.args[1], 0xEF);

  EXPECT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_END);
  EXPECT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_END);
}

TEST(InitSequenceReader, EmptySequenceEnds) {
  InitSequenceReader reader(nullptr, 0);
  InitSequenceEntry entry{};
  EXPECT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_END);
}

// A count above 127 is a real length, not a flag bit.
TEST(InitSequenceReader, HonoursFullCount) {
  uint8_t seq[2 + 253 + 2] = {0x32, 253};
  seq[2 + 253] = 0x29;
  InitSequenceReader reader(seq, sizeof(seq));
  InitSequenceEntry entry{};

  ASSERT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_COMMAND);
  EXPECT_EQ(entry.value, 0x32);
  EXPECT_EQ(entry.count, 253);
  ASSERT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_COMMAND);
  EXPECT_EQ(entry.value, 0x29);
  EXPECT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_END);
}

TEST(InitSequenceReader, TruncatedHeaderIsMalformed) {
  static const uint8_t SEQ[] = {0x11, 0x00, 0x29};
  InitSequenceReader reader(SEQ, sizeof(SEQ));
  InitSequenceEntry entry{};
  ASSERT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_COMMAND);
  EXPECT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_MALFORMED);
  EXPECT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_END);
}

TEST(InitSequenceReader, ShortArgsAreMalformed) {
  static const uint8_t SEQ[] = {0x2A, 0x04, 0x00, 0x00, 0x00};
  InitSequenceReader reader(SEQ, sizeof(SEQ));
  InitSequenceEntry entry{};
  EXPECT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_MALFORMED);
  EXPECT_EQ(reader.next(entry), INIT_SEQUENCE_STEP_END);
}

TEST(InitSequenceReader, CountOverMaxIsMalformed) {
  static const uint8_t SEQ[] = {10, 0xFF, 0x2A, 0x02, 0x00, 0xEF};
  InitSequenceReader reader(SEQ, sizeof(SEQ));
  InitSequenceEntry entry{};
  ASSERT_EQ(reader.next(entry, 1), INIT_SEQUENCE_STEP_DELAY);
  EXPECT_EQ(reader.next(entry, 1), INIT_SEQUENCE_STEP_MALFORMED);
}

}  // namespace esphome::display::testing
