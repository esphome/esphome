#include <gtest/gtest.h>

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <string>

#include "esphome/components/logger/log_buffer.h"

namespace esphome::logger::testing {

static void format_into(LogBuffer &buf, const char *format, ...) {
  va_list args;
  va_start(args, format);
  buf.format_body(format, args);
  va_end(args);
}

// A cut line keeps its whole color reset, and the terminator is never counted in pos
static void expect_reset_and_terminator(const LogBuffer &buf, uint16_t start) {
  ASSERT_LT(buf.pos, buf.size);
  EXPECT_EQ(buf.data[buf.pos], '\0');
  if (buf.size - start > LogBuffer::ANSI_RESET_LEN) {
    ASSERT_GE(buf.pos, start + LogBuffer::ANSI_RESET_LEN);
    EXPECT_EQ(memcmp(buf.data + buf.pos - LogBuffer::ANSI_RESET_LEN, "\033[0m", LogBuffer::ANSI_RESET_LEN), 0);
  }
}

TEST(LogBufferTest, CutLineKeepsColorResetAndTerminator) {
  constexpr uint16_t max_size = 48;
  const std::string text(max_size + 8, 'x');
  for (uint16_t size = 1; size <= max_size; size++) {
    for (uint16_t start = 0; start < size; start++) {
      for (size_t len = 0; len <= text.size(); len++) {
        SCOPED_TRACE(::testing::Message() << "size " << size << " start " << start << " text " << len);
        const std::string body = text.substr(0, len);
        char formatted[max_size], copied[max_size];
        memset(formatted, '#', sizeof(formatted));
        memset(copied, '#', sizeof(copied));
        LogBuffer a{formatted, size, start};
        format_into(a, "%s", body.c_str());
        expect_reset_and_terminator(a, start);
        LogBuffer b{copied, size, start};
        b.write_body(body.data(), static_cast<uint16_t>(len));
        expect_reset_and_terminator(b, start);
        EXPECT_EQ(a.pos, b.pos);
        // The text keeps every character that fits in front of the reset and the terminator
        const int room = size - start - LogBuffer::ANSI_RESET_LEN;
        const size_t kept = room > 0 ? std::min(len, static_cast<size_t>(room - 1)) : 0;
        EXPECT_EQ(a.pos, start + kept + (room > 0 ? LogBuffer::ANSI_RESET_LEN : 0));
        EXPECT_EQ(memcmp(formatted + start, text.data(), kept), 0);
        EXPECT_EQ(memcmp(copied + start, text.data(), kept), 0);
      }
    }
  }
}

TEST(LogBufferTest, CutAfterNewlineDropsIt) {
  char formatted[16], copied[16];
  LogBuffer a{formatted, sizeof(formatted)};
  LogBuffer b{copied, sizeof(copied)};
  // 11 bytes of text fit in 16 after the reset and the terminator; the 11th is the newline
  format_into(a, "%s", "0123456789\nabc");
  b.write_body("0123456789\nabc", 14);
  EXPECT_EQ(std::string(formatted, a.pos), "0123456789\033[0m");
  EXPECT_EQ(std::string(copied, b.pos), "0123456789\033[0m");
}

TEST(LogBufferTest, LineThatFitsIsUnchanged) {
  char data[64];
  LogBuffer buf{data, sizeof(data)};
  format_into(buf, "%s", "hello");
  EXPECT_EQ(std::string(data, buf.pos), "hello\033[0m");
}

}  // namespace esphome::logger::testing
