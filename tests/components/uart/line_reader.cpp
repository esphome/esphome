#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "esphome/components/uart/line_reader.h"

namespace esphome::uart::testing {

using Lines = std::vector<std::string>;

// Feeds a string and collects every completed line.
template<typename R> static Lines feed_all(R &reader, const std::string &input) {
  Lines lines;
  for (char c : input) {
    const char *line = reader.feed(static_cast<uint8_t>(c));
    if (line != nullptr) {
      EXPECT_EQ(reader.length(), 0u);
      lines.emplace_back(line);
    }
  }
  return lines;
}

static_assert(sizeof(LineReader<80>) == 81);
static_assert(sizeof(LineReader<1025>) == 1028);

TEST(LineReaderTest, CrLfLines) {
  LineReader<16> reader;
  EXPECT_EQ(feed_all(reader, "abc\r\ndef\r\n"), (Lines{"abc", "def"}));
}

TEST(LineReaderTest, LfOnlyLines) {
  LineReader<16> reader;
  EXPECT_EQ(feed_all(reader, "abc\ndef\n"), (Lines{"abc", "def"}));
}

TEST(LineReaderTest, CrOnlyLinesWithCrTerminator) {
  LineReader<16, '\r', '\n'> reader;
  EXPECT_EQ(feed_all(reader, "R1234\rR0567\r"), (Lines{"R1234", "R0567"}));
  // The LF of a CRLF pair is ignored with this setup
  EXPECT_EQ(feed_all(reader, "x\r\ny\r\n"), (Lines{"x", "y"}));
}

TEST(LineReaderTest, CrInsideLineIsIgnored) {
  LineReader<16> reader;
  EXPECT_EQ(feed_all(reader, "a\rb\n"), (Lines{"ab"}));
}

TEST(LineReaderTest, EmptyLinesAreReturned) {
  LineReader<16> reader;
  EXPECT_EQ(feed_all(reader, "\r\n\nx\n"), (Lines{"", "", "x"}));
}

TEST(LineReaderTest, LineOfExactlyMaxLengthFits) {
  LineReader<6> reader;
  EXPECT_EQ(feed_all(reader, "12345\n"), (Lines{"12345"}));
  EXPECT_FALSE(reader.overflowed());
}

TEST(LineReaderTest, OverlongLineIsDroppedUpToTerminator) {
  LineReader<6> reader;
  int overflow_count = 0;
  Lines lines;
  for (char c : std::string("123456789\r\nok\n")) {
    const char *line = reader.feed(c);
    if (reader.overflowed()) {
      overflow_count++;
      EXPECT_STREQ(reader.partial(), "12345");
      EXPECT_EQ(reader.length(), 0u);
      reader.log_if_overflowed("line_reader_test");
    }
    if (line != nullptr)
      lines.emplace_back(line);
  }
  EXPECT_EQ(overflow_count, 1);
  EXPECT_EQ(lines, (Lines{"ok"}));
}

TEST(LineReaderTest, ResetAfterOverflowRestartsAtNextByte) {
  LineReader<4> reader;
  Lines lines;
  for (char c : std::string("abcdefg\n")) {
    const char *line = reader.feed(c);
    if (reader.overflowed())
      reader.reset();
    if (line != nullptr)
      lines.emplace_back(line);
  }
  // "abc" fills the buffer, 'd' overflows and is dropped, "efg" is the next line
  EXPECT_EQ(lines, (Lines{"efg"}));
}

TEST(LineReaderTest, RestartWithKeepsOverflowByte) {
  LineReader<4> reader;
  Lines lines;
  for (char c : std::string("abcdef\n")) {
    const char *line = reader.feed(c);
    if (reader.overflowed()) {
      reader.restart_with(c);
    }
    if (line != nullptr) {
      lines.emplace_back(line);
    }
  }
  // 'd' overflows and starts the next line
  EXPECT_EQ(lines, (Lines{"def"}));
}

TEST(LineReaderTest, FeedReportsLength) {
  LineReader<16> reader;
  size_t len = 99;
  char *line = nullptr;
  for (char c : std::string("ab\rc\n")) {
    line = reader.feed(c, len);
  }
  ASSERT_NE(line, nullptr);
  EXPECT_STREQ(line, "abc");
  EXPECT_EQ(len, 3u);
  EXPECT_NE(reader.feed('\n', len), nullptr);
  EXPECT_EQ(len, 0u);
}

TEST(LineReaderTest, PartialForPromptWithoutLineEnd) {
  LineReader<32> reader;
  EXPECT_EQ(feed_all(reader, "DFRobot:/>"), Lines{});
  EXPECT_STREQ(reader.partial(), "DFRobot:/>");
  EXPECT_EQ(reader.length(), 10u);
  reader.reset();
  EXPECT_STREQ(reader.partial(), "");
  EXPECT_EQ(feed_all(reader, "sensorStart\r\n"), (Lines{"sensorStart"}));
}

TEST(LineReaderTest, DiscardLineDropsUntilTerminator) {
  LineReader<16> reader;
  feed_all(reader, "half");
  reader.discard_line();
  EXPECT_EQ(reader.length(), 0u);
  EXPECT_STREQ(reader.partial(), "");
  EXPECT_EQ(feed_all(reader, "rest\nnext\n"), (Lines{"next"}));
}

TEST(LineReaderTest, CustomIgnoreByte) {
  LineReader<8, '\r', static_cast<char>(0xFF)> reader;
  EXPECT_EQ(feed_all(reader, "R12\xFF"
                             "34\r"),
            (Lines{"R1234"}));
}

TEST(LineReaderTest, LineIsWritable) {
  LineReader<16> reader;
  char *line = nullptr;
  for (char c : std::string("a,b\n"))
    line = reader.feed(c);
  ASSERT_NE(line, nullptr);
  line[1] = '\0';
  EXPECT_STREQ(line, "a");
}

}  // namespace esphome::uart::testing
