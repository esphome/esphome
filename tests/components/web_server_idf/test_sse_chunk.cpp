#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

#include "esphome/components/web_server_idf/sse_chunk.h"

namespace esphome::web_server_idf::testing {

// The chunk as try_send_nodefer lays it out: header, fields, first "data: ", then the pieces
static std::string build_chunk(const char *message, size_t message_len, const char *event, uint32_t id,
                               uint32_t reconnect) {
  char prefix[128];
  const size_t prefix_len = build_chunk_prefix(prefix, sizeof(prefix), event, id, reconnect, message != nullptr);
  if (message == nullptr && prefix_len == CHUNK_HDR_LEN) {
    return "";
  }
  std::string out(prefix, prefix_len);
  for_each_chunk_piece(
      message, message_len,
      [](void *ctx, const char *piece, size_t len) { static_cast<std::string *>(ctx)->append(piece, len); }, &out);
  write_chunk_header(prefix, out.size() - CHUNK_HDR_LEN - CHUNK_END_LEN);
  out.replace(0, CHUNK_HDR_LEN, prefix, CHUNK_HDR_LEN);
  return out;
}

// The std::string builder this framing replaced, kept as the reference for the wire format
static std::string reference_chunk(const char *message, size_t message_len, const char *event, uint32_t id,
                                   uint32_t reconnect) {
  std::string buf = "        \r\n";
  char num[32];
  if (reconnect)
    buf.append(num, snprintf(num, sizeof(num), "retry: %u\r\n", reconnect));
  if (id)
    buf.append(num, snprintf(num, sizeof(num), "id: %u\r\n", id));
  if (event && *event)
    buf.append("event: ").append(event).append("\r\n");
  if (message) {
    const char *line_start = message;
    const char *msg_end = message + message_len;
    const char *next_n = static_cast<const char *>(memchr(message, '\n', message_len));
    const char *next_r = static_cast<const char *>(memchr(message, '\r', message_len));
    if (next_n == nullptr && next_r == nullptr) {
      buf.append("data: ").append(message, message_len).append("\r\n\r\n");
    } else {
      while (line_start <= msg_end) {
        const char *line_end;
        const char *next_line;
        if (next_n == nullptr && next_r == nullptr) {
          buf.append("data: ").append(line_start, msg_end - line_start).append("\r\n");
          break;
        }
        if (next_n != nullptr && next_r != nullptr) {
          if (next_r + 1 == next_n) {
            line_end = next_r;
            next_line = next_n + 1;
          } else {
            line_end = (next_r < next_n) ? next_r : next_n;
            next_line = line_end + 1;
          }
        } else if (next_n != nullptr) {
          line_end = next_n;
          next_line = next_n + 1;
        } else {
          line_end = next_r;
          next_line = next_r + 1;
        }
        buf.append("data: ").append(line_start, line_end - line_start).append("\r\n");
        line_start = next_line;
        if (line_start >= msg_end)
          break;
        next_n = static_cast<const char *>(memchr(line_start, '\n', msg_end - line_start));
        next_r = static_cast<const char *>(memchr(line_start, '\r', msg_end - line_start));
      }
      buf.append("\r\n");
    }
  }
  if (buf.size() == 10)
    return "";
  buf.append("\r\n");
  char len[9];
  snprintf(len, sizeof(len), "%08x", static_cast<unsigned>(buf.size() - 2 - 10));
  buf.replace(0, 8, len, 8);
  return buf;
}

static void expect_same(const std::string &message, const char *event, uint32_t id, uint32_t reconnect) {
  const std::string got = build_chunk(message.data(), message.size(), event, id, reconnect);
  const std::string want = reference_chunk(message.data(), message.size(), event, id, reconnect);
  EXPECT_EQ(got, want) << "message=[" << message << "] event=" << (event ? event : "null") << " id=" << id
                       << " retry=" << reconnect;
}

TEST(SseChunk, NullMessageHasNoDataLineAndNoBlankLine) {
  EXPECT_EQ(build_chunk(nullptr, 0, "ping", 7, 30000), reference_chunk(nullptr, 0, "ping", 7, 30000));
  EXPECT_EQ(build_chunk(nullptr, 0, "ping", 7, 30000), "00000022\r\nretry: 30000\r\nid: 7\r\nevent: ping\r\n\r\n");
  EXPECT_EQ(build_chunk(nullptr, 0, nullptr, 0, 0), "");
}

TEST(SseChunk, SingleLine) {
  EXPECT_EQ(build_chunk("{}", 2, "state", 0, 0), "0000001a\r\nevent: state\r\ndata: {}\r\n\r\n\r\n");
  expect_same("", "state", 0, 0);
  expect_same(R"({"id":"light-x"})", "state_detail_all", 0, 0);
}

TEST(SseChunk, LineBreaks) {
  for (const char *m : {"a\n", "a\r", "a\r\n", "\n", "\r\n", "\r", "a\n\nb", "a\r\rb", "a\n\rb", "a\r\n\r\nb", "\n\n",
                        "x\r\n", "\r\nx", "one\ntwo\nthree", "tail\r\n\r\n"}) {
    expect_same(m, "log", 1234, 0);
  }
  EXPECT_EQ(build_chunk("a\r\nb", 4, nullptr, 0, 0), "00000014\r\ndata: a\r\ndata: b\r\n\r\n\r\n");
  EXPECT_EQ(build_chunk("a\n\rb", 4, nullptr, 0, 0), "0000001c\r\ndata: a\r\ndata: \r\ndata: b\r\n\r\n\r\n");
}

TEST(SseChunk, MatchesReferenceOnRandomMessages) {
  std::mt19937 rng(1234);  // NOLINT(cert-msc32-c,cert-msc51-cpp,bugprone-random-generator-seed) reproducible
  const char *events[] = {nullptr, "", "ping", "state", "log", "state_detail_all", "sorting_group"};
  const char alphabet[] = "ab{}\":,\n\r ";
  for (int i = 0; i < 20000; i++) {
    std::string m;
    const size_t len = rng() % 120;
    for (size_t k = 0; k < len; k++)
      m += alphabet[rng() % (sizeof(alphabet) - 1)];
    expect_same(m, events[rng() % 7], (rng() % 3 == 0) ? 0 : rng(), (rng() % 4 == 0) ? 30000 : 0);
  }
}

}  // namespace esphome::web_server_idf::testing
