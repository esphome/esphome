#include <gtest/gtest.h>

#include <sys/uio.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "esphome/components/web_server_idf/sse_chunk.h"
#include "esphome/components/web_server_idf/stream_head.h"

namespace esphome::web_server_idf::testing {

// Collects every batch write_stream_head() sends
struct Capture {
  std::string bytes;
  std::vector<int> batch_sizes;
  int fail_after_batches{-1};

  bool operator()(struct iovec *iov, int count) {
    if (static_cast<int>(this->batch_sizes.size()) == this->fail_after_batches) {
      return false;
    }
    this->batch_sizes.push_back(count);
    for (int i = 0; i < count; i++) {
      this->bytes.append(static_cast<const char *>(iov[i].iov_base), iov[i].iov_len);
    }
    return true;
  }
};

template<int MaxIov = STREAM_HEAD_MAX_IOV>
static std::string build_head(Capture &cap, const char *status, const char *content_type,
                              std::span<const HttpHeader> headers, std::span<const HttpHeader> defaults,
                              const char *body) {
  EXPECT_TRUE((write_stream_head<struct iovec, MaxIov>(cap, status, content_type, headers, defaults, body)));
  return cap.bytes;
}

// What httpd_resp_send_chunk() put on the wire for its first chunk (esp_http_server/src/httpd_txrx.c in
// ESP-IDF 5.5): the chunked head with Transfer-Encoding, each httpd_resp_set_hdr() header, the blank line,
// then "%lx\r\n", the chunk data and "\r\n".
static std::string reference_send_chunk(const char *status, const char *content_type,
                                        const std::vector<HttpHeader> &resp_hdrs, const char *buf, size_t buf_len) {
  char head[256];
  std::string out(
      head, snprintf(head, sizeof(head), "HTTP/1.1 %s\r\nContent-Type: %s\r\nTransfer-Encoding: chunked\r\n", status,
                     content_type));
  for (const auto &hdr : resp_hdrs) {
    out.append(hdr.name).append(": ").append(hdr.value).append("\r\n");
  }
  out.append("\r\n");
  char len_str[10];
  out.append(len_str, snprintf(len_str, sizeof(len_str), "%lx\r\n", static_cast<long>(buf_len)));
  out.append(buf, buf_len);
  out.append("\r\n");
  return out;
}

static constexpr HttpHeader DEFAULTS[] = {
    {"Access-Control-Allow-Origin", "*"},
    {"Access-Control-Allow-Private-Network", "true"},
};

TEST(StreamHead, SseHeadMatchesHttpdRespSendChunk) {
  // The old event source set Cache-Control and Connection, then the default headers, and sent "\r\n" as a chunk
  const std::string expected = reference_send_chunk(
      "200 OK", "text/event-stream",
      {{"Cache-Control", "no-cache"}, {"Connection", "keep-alive"}, DEFAULTS[0], DEFAULTS[1]}, "\r\n", 2);
  Capture cap;
  EXPECT_EQ(build_head(cap, SSE_STATUS, SSE_CONTENT_TYPE, SSE_HEADERS, DEFAULTS, SSE_FIRST_CHUNK), expected);
}

TEST(StreamHead, SseHeadWithoutDefaultHeaders) {
  const std::string expected = reference_send_chunk(
      "200 OK", "text/event-stream", {{"Cache-Control", "no-cache"}, {"Connection", "keep-alive"}}, "\r\n", 2);
  Capture cap;
  EXPECT_EQ(build_head(cap, SSE_STATUS, SSE_CONTENT_TYPE, SSE_HEADERS, {}, SSE_FIRST_CHUNK), expected);
}

TEST(StreamHead, RawHeadWithCustomHeadersAndNoBody) {
  static constexpr HttpHeader HEADERS[] = {
      {"Cache-Control", "no-store"},
      {"X-Stream", "audio"},
  };
  Capture cap;
  EXPECT_EQ(build_head(cap, "200 OK", "audio/wav", HEADERS, {}, nullptr), "HTTP/1.1 200 OK\r\n"
                                                                          "Content-Type: audio/wav\r\n"
                                                                          "Cache-Control: no-store\r\n"
                                                                          "X-Stream: audio\r\n"
                                                                          "\r\n");
}

TEST(StreamHead, BareHead) {
  Capture cap;
  EXPECT_EQ(build_head(cap, "204 No Content", "text/plain", {}, {}, nullptr),
            "HTTP/1.1 204 No Content\r\nContent-Type: text/plain\r\n\r\n");
  EXPECT_EQ(cap.batch_sizes, std::vector<int>{5});
}

TEST(StreamHead, ManyHeadersSplitIntoBatchesWithSameBytes) {
  // 4 pieces per header: 4 + 12 * 4 + 2 = 54 pieces, more than two batches hold
  static constexpr HttpHeader HEADERS[] = {
      {"X-Header-0", "value-0"}, {"X-Header-1", "value-1"}, {"X-Header-2", "value-2"}, {"X-Header-3", "value-3"},
      {"X-Header-4", "value-4"}, {"X-Header-5", "value-5"}, {"X-Header-6", "value-6"}, {"X-Header-7", "value-7"},
      {"X-Header-8", "value-8"}, {"X-Header-9", "value-9"},
  };
  const std::span<const HttpHeader> headers{HEADERS};

  Capture batched;
  const std::string bytes = build_head(batched, "200 OK", "text/plain", headers, DEFAULTS, "body");
  Capture whole;
  EXPECT_EQ(bytes, build_head<64>(whole, "200 OK", "text/plain", headers, DEFAULTS, "body"));
  EXPECT_EQ(whole.batch_sizes, std::vector<int>{54});
  EXPECT_EQ(batched.batch_sizes, (std::vector<int>{STREAM_HEAD_MAX_IOV, STREAM_HEAD_MAX_IOV, 6}));

  std::string expected = "HTTP/1.1 200 OK\r\nContent-Type: text/plain";
  for (const auto &h : headers) {
    expected.append("\r\n").append(h.name).append(": ").append(h.value);
  }
  for (const auto &h : DEFAULTS) {
    expected.append("\r\n").append(h.name).append(": ").append(h.value);
  }
  expected.append("\r\n\r\nbody");
  EXPECT_EQ(bytes, expected);
}

TEST(StreamHead, BatchBoundary) {
  // 4 + 4 * 4 + 1 + 1 = 22 pieces fit one batch; a fifth header makes 4 + 5 * 4 = 24 before the blank line
  static constexpr HttpHeader HEADERS[] = {{"A", "1"}, {"B", "2"}, {"C", "3"}, {"D", "4"}, {"E", "5"}};
  Capture four;
  build_head(four, "200 OK", "text/plain", std::span(HEADERS, 4), {}, "x");
  EXPECT_EQ(four.batch_sizes, std::vector<int>{22});
  Capture five;
  const std::string bytes = build_head(five, "200 OK", "text/plain", HEADERS, {}, "x");
  EXPECT_EQ(five.batch_sizes, (std::vector<int>{STREAM_HEAD_MAX_IOV, 2}));
  EXPECT_EQ(bytes, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nA: 1\r\nB: 2\r\nC: 3\r\nD: 4\r\nE: 5\r\n\r\nx");
}

TEST(StreamHead, FailedSendSkipsLaterBatches) {
  std::vector<HttpHeader> headers(10, HttpHeader{"X", "y"});
  Capture cap;
  cap.fail_after_batches = 1;
  EXPECT_FALSE(write_stream_head<struct iovec>(cap, "200 OK", "text/plain", headers, {}, nullptr));
  EXPECT_EQ(cap.batch_sizes.size(), 1u);
}

// Three elements of 3, 4 and 5 bytes
struct IovFixture {
  char a[4] = "abc";
  char b[5] = "defg";
  char c[6] = "hijkl";
  struct iovec iov[3] = {{a, 3}, {b, 4}, {c, 5}};
  struct iovec *cur = iov;
  int count = 3;

  std::string rest() const {
    std::string out;
    for (int i = 0; i < this->count; i++) {
      out.append(static_cast<const char *>(this->cur[i].iov_base), this->cur[i].iov_len);
    }
    return out;
  }
};

TEST(AdvanceIovecs, ZeroLengthWriteChangesNothing) {
  IovFixture f;
  advance_iovecs(f.cur, f.count, 0);
  EXPECT_EQ(f.cur, f.iov);
  EXPECT_EQ(f.count, 3);
  EXPECT_EQ(f.rest(), "abcdefghijkl");
}

TEST(AdvanceIovecs, PartialInsideFirstElement) {
  IovFixture f;
  advance_iovecs(f.cur, f.count, 2);
  EXPECT_EQ(f.cur, f.iov);
  EXPECT_EQ(f.count, 3);
  EXPECT_EQ(f.cur->iov_len, 1u);
  EXPECT_EQ(f.rest(), "cdefghijkl");
}

TEST(AdvanceIovecs, ExactlyOnElementBoundary) {
  IovFixture f;
  advance_iovecs(f.cur, f.count, 3);
  EXPECT_EQ(f.cur, &f.iov[1]);
  EXPECT_EQ(f.count, 2);
  EXPECT_EQ(f.rest(), "defghijkl");
}

TEST(AdvanceIovecs, AcrossElementBoundaries) {
  IovFixture f;
  advance_iovecs(f.cur, f.count, 9);
  EXPECT_EQ(f.cur, &f.iov[2]);
  EXPECT_EQ(f.count, 1);
  EXPECT_EQ(f.rest(), "jkl");
}

TEST(AdvanceIovecs, RepeatedShortWrites) {
  IovFixture f;
  std::string sent;
  while (f.count > 0) {
    const std::string before = f.rest();
    advance_iovecs(f.cur, f.count, 2);
    sent += before.substr(0, before.size() - f.rest().size());
  }
  EXPECT_EQ(sent, "abcdefghijkl");
}

TEST(AdvanceIovecs, EverythingWritten) {
  IovFixture f;
  advance_iovecs(f.cur, f.count, 12);
  EXPECT_EQ(f.count, 0);
  EXPECT_EQ(f.rest(), "");
}

TEST(AdvanceIovecs, SkipsZeroLengthElements) {
  char a[] = "ab";
  char c[] = "cd";
  struct iovec iov[3] = {{a, 2}, {nullptr, 0}, {c, 2}};
  struct iovec *cur = iov;
  int count = 3;
  advance_iovecs(cur, count, 2);
  EXPECT_EQ(cur, &iov[2]);
  EXPECT_EQ(count, 1);
}

static constexpr uint32_t TIMEOUT = 20000;

TEST(StallClock, NotStarted) {
  StallClock clock;
  EXPECT_FALSE(clock.is_running());
  EXPECT_FALSE(clock.is_expired(0, TIMEOUT));
  EXPECT_FALSE(clock.is_expired(UINT32_MAX, TIMEOUT));
}

TEST(StallClock, ExpiresAtExactlyTheTimeout) {
  StallClock clock;
  clock.start(1000);
  EXPECT_TRUE(clock.is_running());
  EXPECT_FALSE(clock.is_expired(1000, TIMEOUT));
  EXPECT_FALSE(clock.is_expired(1000 + TIMEOUT - 1, TIMEOUT));
  EXPECT_TRUE(clock.is_expired(1000 + TIMEOUT, TIMEOUT));
  EXPECT_TRUE(clock.is_expired(1000 + TIMEOUT + 1, TIMEOUT));
  EXPECT_EQ(clock.elapsed(1000 + TIMEOUT), TIMEOUT);
}

TEST(StallClock, RunningClockKeepsItsStart) {
  StallClock clock;
  clock.start(1000);
  clock.start(5000);
  EXPECT_TRUE(clock.is_expired(1000 + TIMEOUT, TIMEOUT));
}

TEST(StallClock, Cleared) {
  StallClock clock;
  clock.start(1000);
  clock.clear();
  EXPECT_FALSE(clock.is_running());
  EXPECT_FALSE(clock.is_expired(1000 + TIMEOUT, TIMEOUT));
  clock.start(50000);
  EXPECT_FALSE(clock.is_expired(50000 + TIMEOUT - 1, TIMEOUT));
  EXPECT_TRUE(clock.is_expired(50000 + TIMEOUT, TIMEOUT));
}

TEST(StallClock, StartAtZeroStillRuns) {
  StallClock clock;
  clock.start(0);
  EXPECT_TRUE(clock.is_running());
  // Recorded as 1, so it expires one millisecond late
  EXPECT_FALSE(clock.is_expired(TIMEOUT, TIMEOUT));
  EXPECT_TRUE(clock.is_expired(TIMEOUT + 1, TIMEOUT));
}

TEST(StallClock, AcrossMillisWraparound) {
  StallClock clock;
  const uint32_t start = UINT32_MAX - 5000;
  clock.start(start);
  EXPECT_FALSE(clock.is_expired(UINT32_MAX, TIMEOUT));
  EXPECT_FALSE(clock.is_expired(start + TIMEOUT - 1, TIMEOUT));  // wrapped to 14998
  EXPECT_TRUE(clock.is_expired(start + TIMEOUT, TIMEOUT));
  EXPECT_EQ(clock.elapsed(start + TIMEOUT), TIMEOUT);
}

}  // namespace esphome::web_server_idf::testing
