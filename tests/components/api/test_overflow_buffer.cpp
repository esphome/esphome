#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <vector>

#include "esphome/components/api/api_overflow_buffer.h"

#ifdef USE_HOST
namespace esphome::api::testing {

// Idle cost is the buffer plus one word of bookkeeping
static_assert(sizeof(APIOverflowBuffer) <= sizeof(APIBuffer) + sizeof(void *));

// Exposes storage so tests can check it is reused, not reallocated
class TestOverflowBuffer : public APIOverflowBuffer {
 public:
  using APIOverflowBuffer::LEN_PREFIX;
  using APIOverflowBuffer::MAX_BYTES;
  using APIOverflowBuffer::MAX_LONE_BYTES;
  struct Storage {
    size_t capacity;
    const uint8_t *data;
    bool operator==(const Storage &) const = default;
  };
  size_t capacity() const { return this->buf_.capacity(); }
  Storage storage() const { return {this->buf_.capacity(), this->buf_.data()}; }
  uint8_t count() const { return this->count_; }
  size_t live() const { return this->buf_.size() - this->head_; }
  /// Simulates a socket write inside try_drain() re-entering the send path
  void set_draining(bool draining) { this->draining_ = draining; }
};

static std::vector<uint8_t> make_message(size_t len, uint8_t seed) {
  std::vector<uint8_t> msg(len);
  for (size_t i = 0; i < len; i++)
    msg[i] = static_cast<uint8_t>(seed + i);
  return msg;
}

static bool enqueue(TestOverflowBuffer &buf, const std::vector<uint8_t> &msg, uint16_t skip = 0) {
  struct iovec iov = {const_cast<uint8_t *>(msg.data()), msg.size()};
  return buf.enqueue_iov(&iov, 1, static_cast<uint16_t>(msg.size()), skip);
}

static void append(std::vector<uint8_t> &dst, const std::vector<uint8_t> &src, size_t skip = 0) {
  dst.insert(dst.end(), src.begin() + skip, src.end());
}

static std::vector<uint8_t> concat(std::initializer_list<std::vector<uint8_t>> parts) {
  std::vector<uint8_t> out;
  for (const auto &part : parts)
    append(out, part);
  return out;
}

/// The pipe delivers the filler first, then the drained messages.
static void expect_after_filler(const std::vector<uint8_t> &received, size_t filler,
                                const std::vector<uint8_t> &expected) {
  ASSERT_EQ(received.size(), filler + expected.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), received.begin() + filler));
}

// Non-blocking socket pair with small buffers, so the writer fills like a stalled TCP connection
class OverflowBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int fds[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    int size = 4096;
    ASSERT_EQ(::setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)), 0);
    ASSERT_EQ(::setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)), 0);
    ASSERT_EQ(::fcntl(fds[1], F_SETFL, O_NONBLOCK), 0);
    this->reader_ = fds[1];
    this->sock_ = std::make_unique<socket::Socket>(fds[0]);
    ASSERT_EQ(this->sock_->setblocking(false), 0);
  }
  void TearDown() override { ::close(this->reader_); }

  /// Write filler until the socket refuses; returns the bytes accepted
  size_t fill_pipe_() {
    uint8_t junk[512];
    std::memset(junk, 0xEE, sizeof(junk));
    size_t total = 0;
    for (;;) {
      ssize_t written = this->sock_->write(junk, sizeof(junk));
      if (written <= 0)
        break;
      total += static_cast<size_t>(written);
    }
    return total;
  }

  /// Append whatever the pipe currently holds.
  void read_into_(std::vector<uint8_t> &out) {
    uint8_t tmp[1024];
    for (;;) {
      ssize_t n = ::read(this->reader_, tmp, sizeof(tmp));
      if (n <= 0)
        break;
      out.insert(out.end(), tmp, tmp + n);
    }
  }

  /// Drain once; a refusal must be a would-block, never a hard error.
  ssize_t drain_(TestOverflowBuffer &buf) {
    ssize_t sent = buf.try_drain(this->sock_.get());
    if (sent == -1) {
      EXPECT_TRUE(errno == EWOULDBLOCK || errno == EAGAIN);
    }
    return sent;
  }

  /// Read and drain until the backlog is empty; returns all bytes received
  std::vector<uint8_t> drain_all_(TestOverflowBuffer &buf) {
    std::vector<uint8_t> received;
    for (int i = 0; i < 10000 && !buf.empty(); i++) {
      this->read_into_(received);
      // A hard socket error would never clear the backlog; stop instead of spinning
      if (this->drain_(buf) == -1 && errno != EWOULDBLOCK && errno != EAGAIN)
        break;
    }
    EXPECT_TRUE(buf.empty());
    this->read_into_(received);
    return received;
  }

  struct Stall {
    size_t filler;
    std::vector<uint8_t> first, second, received;
    TestOverflowBuffer::Storage before;
  };
  /// Park two messages, then drain the first fully and the second part way
  void stall_mid_message_(TestOverflowBuffer &buf, Stall &s) {
    s.filler = this->fill_pipe_();
    s.first = make_message(1500, 20);
    // Larger than the whole pipe, so a drain always stops inside it
    s.second = make_message(std::min<size_t>(s.filler * 3, 12000), 60);
    ASSERT_GT(s.second.size(), s.filler);
    ASSERT_TRUE(enqueue(buf, s.first));
    ASSERT_TRUE(enqueue(buf, s.second));
    s.before = buf.storage();
    this->read_into_(s.received);
    ASSERT_GT(this->drain_(buf), 0);
    ASSERT_EQ(buf.count(), 1);
  }

  int reader_{-1};
  std::unique_ptr<socket::Socket> sock_;
};

TEST_F(OverflowBufferTest, IdleBufferOwnsNoStorage) {
  TestOverflowBuffer buf;
  EXPECT_TRUE(buf.empty());
  EXPECT_EQ(buf.capacity(), 0u);
  EXPECT_EQ(buf.storage().data, nullptr);
}

TEST_F(OverflowBufferTest, StorageIsReusedAcrossStalls) {
  TestOverflowBuffer buf;
  auto msg = make_message(1000, 1);

  size_t filler = this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, msg));
  const auto storage = buf.storage();
  EXPECT_GE(storage.capacity, msg.size() + TestOverflowBuffer::LEN_PREFIX);

  for (int stall = 0; stall < 5; stall++) {
    expect_after_filler(this->drain_all_(buf), filler, msg);
    EXPECT_TRUE(buf.empty());
    // Same allocation every time: no free, no new allocation
    EXPECT_EQ(buf.storage(), storage);

    filler = this->fill_pipe_();
    ASSERT_TRUE(enqueue(buf, msg));
    EXPECT_EQ(buf.storage(), storage);
  }
}

TEST_F(OverflowBufferTest, ReleaseWhileQueuedFreesOnceDrained) {
  TestOverflowBuffer buf;
  auto msg = make_message(1000, 7);

  size_t filler = this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, msg));
  const size_t capacity = buf.capacity();

  // Requested while the backlog still holds data: storage must stay until sent
  buf.release();
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.capacity(), capacity);

  expect_after_filler(this->drain_all_(buf), filler, msg);
  EXPECT_TRUE(buf.empty());
  EXPECT_EQ(buf.capacity(), 0u);
  EXPECT_EQ(buf.storage().data, nullptr);

  // A later stall allocates again and keeps it, since nobody asked for a release
  filler = this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, msg));
  EXPECT_GT(buf.capacity(), 0u);
  this->drain_all_(buf);
  EXPECT_GT(buf.capacity(), 0u);
}

TEST_F(OverflowBufferTest, ReleaseWhenEmptyFreesImmediately) {
  TestOverflowBuffer buf;
  auto msg = make_message(100, 3);

  this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, msg));
  this->drain_all_(buf);
  EXPECT_GT(buf.capacity(), 0u);

  buf.release();
  EXPECT_EQ(buf.capacity(), 0u);
  EXPECT_EQ(buf.storage().data, nullptr);
}

TEST_F(OverflowBufferTest, PreservesOrderAndSkipsSentPrefix) {
  TestOverflowBuffer buf;
  auto first = make_message(700, 10);
  auto second_a = make_message(300, 50);
  auto second_b = make_message(400, 90);
  auto third = make_message(200, 130);

  size_t filler = this->fill_pipe_();
  // 100 bytes of the first message were already accepted by the socket
  ASSERT_TRUE(enqueue(buf, first, 100));
  // Two iovecs with the skip covering all of the first one plus part of the second
  struct iovec iov[2] = {{second_a.data(), second_a.size()}, {second_b.data(), second_b.size()}};
  const uint16_t second_skip = static_cast<uint16_t>(second_a.size() + 5);
  ASSERT_TRUE(buf.enqueue_iov(iov, 2, static_cast<uint16_t>(second_a.size() + second_b.size()), second_skip));
  ASSERT_TRUE(enqueue(buf, third));
  EXPECT_EQ(buf.count(), 3);

  // Nothing can go out while the pipe is full
  EXPECT_EQ(this->drain_(buf), -1);
  EXPECT_EQ(buf.count(), 3);

  std::vector<uint8_t> expected;
  append(expected, first, 100);
  append(expected, second_b, 5);
  append(expected, third);
  expect_after_filler(this->drain_all_(buf), filler, expected);
}

TEST_F(OverflowBufferTest, RefusesWhenQueueIsFull) {
  TestOverflowBuffer buf;
  auto msg = make_message(16, 1);

  size_t filler = this->fill_pipe_();
  for (int i = 0; i < API_MAX_SEND_QUEUE; i++) {
    ASSERT_TRUE(enqueue(buf, msg)) << "message " << i;
  }
  EXPECT_FALSE(enqueue(buf, msg));
  EXPECT_EQ(buf.count(), API_MAX_SEND_QUEUE);

  // Draining frees the slots again
  std::vector<uint8_t> expected;
  for (int i = 0; i < API_MAX_SEND_QUEUE; i++)
    append(expected, msg);
  expect_after_filler(this->drain_all_(buf), filler, expected);
  this->fill_pipe_();
  EXPECT_TRUE(enqueue(buf, msg));
  EXPECT_EQ(buf.count(), 1);
}

TEST_F(OverflowBufferTest, SkipAtIovecBoundary) {
  TestOverflowBuffer buf;
  auto sent = make_message(300, 50);
  auto unsent = make_message(400, 90);

  size_t filler = this->fill_pipe_();
  // The skip covers the first iovec exactly, so only the second is copied
  struct iovec iov[2] = {{sent.data(), sent.size()}, {unsent.data(), unsent.size()}};
  ASSERT_TRUE(
      buf.enqueue_iov(iov, 2, static_cast<uint16_t>(sent.size() + unsent.size()), static_cast<uint16_t>(sent.size())));
  EXPECT_EQ(buf.live(), unsent.size() + TestOverflowBuffer::LEN_PREFIX);
  expect_after_filler(this->drain_all_(buf), filler, unsent);
}

TEST_F(OverflowBufferTest, AppendsBehindSentPrefixWhenItFits) {
  TestOverflowBuffer buf;
  size_t filler = this->fill_pipe_();
  auto first = make_message(200, 20);
  // Size the second message so the two land half way into a 256 byte step,
  // leaving exactly 128 bytes of slack whatever the pipe accepted
  const size_t base = std::min<size_t>(filler * 3, 12000);
  const size_t second_len = (base / 256 + 1) * 256 + 128 - first.size() - 2 * TestOverflowBuffer::LEN_PREFIX;
  auto second = make_message(second_len, 60);
  ASSERT_GT(second.size(), filler);
  ASSERT_TRUE(enqueue(buf, first));
  ASSERT_TRUE(enqueue(buf, second));
  const auto storage = buf.storage();
  const size_t slack = storage.capacity - first.size() - second.size() - 2 * TestOverflowBuffer::LEN_PREFIX;
  ASSERT_EQ(slack, 128u);
  auto third = make_message(slack - TestOverflowBuffer::LEN_PREFIX, 200);

  std::vector<uint8_t> received;
  this->read_into_(received);
  ASSERT_GT(this->drain_(buf), 0);
  ASSERT_EQ(buf.count(), 1);
  const size_t live = buf.live();

  // Fits in the tail, so the sent prefix is left alone
  ASSERT_TRUE(enqueue(buf, third));
  EXPECT_EQ(buf.storage(), storage);
  EXPECT_EQ(buf.live(), live + third.size() + TestOverflowBuffer::LEN_PREFIX);

  append(received, this->drain_all_(buf));
  expect_after_filler(received, filler, concat({first, second, third}));
}

TEST_F(OverflowBufferTest, ReleaseSurvivesFurtherEnqueues) {
  TestOverflowBuffer buf;
  auto first = make_message(300, 7);
  auto second = make_message(300, 70);

  size_t filler = this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, first));
  buf.release();
  ASSERT_TRUE(enqueue(buf, second));
  EXPECT_GT(buf.capacity(), 0u);

  expect_after_filler(this->drain_all_(buf), filler, concat({first, second}));
  EXPECT_EQ(buf.capacity(), 0u);
}

TEST_F(OverflowBufferTest, RefusesWhenByteLimitIsExceeded) {
  TestOverflowBuffer buf;
  // Two of these fill the byte budget exactly, well before the slot count is reached
  static_assert(API_MAX_SEND_QUEUE >= 3);
  auto msg = make_message(TestOverflowBuffer::MAX_BYTES / 2 - TestOverflowBuffer::LEN_PREFIX, 1);

  this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, msg));
  ASSERT_TRUE(enqueue(buf, msg));
  EXPECT_FALSE(enqueue(buf, msg));
  EXPECT_EQ(buf.count(), 2);
}

TEST_F(OverflowBufferTest, LoneMessageMayExceedByteLimit) {
  TestOverflowBuffer buf;
  // The oversized message must still fit under the lone message ceiling
  static_assert(TestOverflowBuffer::MAX_BYTES + 100 + TestOverflowBuffer::LEN_PREFIX <=
                TestOverflowBuffer::MAX_LONE_BYTES);
  auto big = make_message(TestOverflowBuffer::MAX_BYTES + 100, 5);
  auto small = make_message(16, 9);

  // Refusing the only message would drop the connection for nothing
  size_t filler = this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, big));
  EXPECT_EQ(buf.count(), 1);
  // With a backlog present the byte limit applies again
  EXPECT_FALSE(enqueue(buf, small));
  EXPECT_EQ(buf.count(), 1);

  expect_after_filler(this->drain_all_(buf), filler, big);
}

TEST_F(OverflowBufferTest, LoneMessageAboveOffsetLimitIsRefused) {
  TestOverflowBuffer buf;
  // Payload plus prefix is past the lone message ceiling
  auto msg = make_message(TestOverflowBuffer::MAX_LONE_BYTES, 3);

  this->fill_pipe_();
  EXPECT_FALSE(enqueue(buf, msg));
  EXPECT_TRUE(buf.empty());
  EXPECT_EQ(buf.capacity(), 0u);
}

TEST_F(OverflowBufferTest, HardSocketErrorLeavesBacklogIntact) {
  TestOverflowBuffer buf;
  auto msg = make_message(300, 40);

  this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, msg));
  // A closed socket fails every write outright, unlike a full one
  ASSERT_EQ(this->sock_->close(), 0);

  errno = 0;
  EXPECT_EQ(buf.try_drain(this->sock_.get()), -1);
  EXPECT_NE(errno, EWOULDBLOCK);
  EXPECT_NE(errno, EAGAIN);
  EXPECT_EQ(buf.count(), 1);
  EXPECT_EQ(buf.live(), msg.size() + TestOverflowBuffer::LEN_PREFIX);
}

TEST_F(OverflowBufferTest, GrowsWhileReclaimingSentPrefix) {
  TestOverflowBuffer buf;
  Stall s;
  ASSERT_NO_FATAL_FAILURE(this->stall_mid_message_(buf, s));

  // One byte too many to fit even after the sent prefix is reclaimed: grows in one copy
  auto third = make_message(s.before.capacity - buf.live() + 1, 200);
  ASSERT_TRUE(enqueue(buf, third));
  EXPECT_GT(buf.capacity(), s.before.capacity);
  EXPECT_EQ(buf.count(), 2);

  append(s.received, this->drain_all_(buf));
  expect_after_filler(s.received, s.filler, concat({s.first, s.second, third}));
}

TEST_F(OverflowBufferTest, NestedDrainMakesNoProgress) {
  TestOverflowBuffer buf;
  auto msg = make_message(300, 40);

  size_t filler = this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, msg));
  std::vector<uint8_t> received;
  this->read_into_(received);

  // Room is available, but a nested drain must leave the outer one's message alone
  buf.set_draining(true);
  EXPECT_EQ(this->drain_(buf), 0);
  EXPECT_EQ(buf.count(), 1);
  std::vector<uint8_t> nothing;
  this->read_into_(nothing);
  EXPECT_TRUE(nothing.empty());

  buf.set_draining(false);
  append(received, this->drain_all_(buf));
  expect_after_filler(received, filler, msg);
}

TEST_F(OverflowBufferTest, NestedEnqueueAppendsWithinCapacity) {
  TestOverflowBuffer buf;
  auto first = make_message(500, 10);
  auto second = make_message(4, 90);

  size_t filler = this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, first));
  const auto storage = buf.storage();
  ASSERT_GE(storage.capacity, first.size() + second.size() + 2 * TestOverflowBuffer::LEN_PREFIX);

  buf.set_draining(true);
  EXPECT_TRUE(enqueue(buf, second));
  EXPECT_EQ(buf.count(), 2);
  EXPECT_EQ(buf.storage(), storage);
  buf.set_draining(false);

  expect_after_filler(this->drain_all_(buf), filler, concat({first, second}));
}

TEST_F(OverflowBufferTest, NestedEnqueueRefusesToGrow) {
  TestOverflowBuffer buf;
  auto first = make_message(500, 10);
  auto second = make_message(100, 90);

  size_t filler = this->fill_pipe_();
  ASSERT_TRUE(enqueue(buf, first));
  const auto storage = buf.storage();
  ASSERT_LT(storage.capacity, first.size() + second.size() + 2 * TestOverflowBuffer::LEN_PREFIX);

  // Growing would free the bytes the outer write() is sending from
  buf.set_draining(true);
  EXPECT_FALSE(enqueue(buf, second));
  EXPECT_EQ(buf.count(), 1);
  EXPECT_EQ(buf.storage(), storage);
  buf.set_draining(false);

  expect_after_filler(this->drain_all_(buf), filler, first);
}

TEST_F(OverflowBufferTest, NestedEnqueueRefusesToCompact) {
  TestOverflowBuffer buf;
  Stall s;
  ASSERT_NO_FATAL_FAILURE(this->stall_mid_message_(buf, s));
  auto third = make_message(1000, 200);

  // Sliding the remainder down would move the bytes the outer write() points at
  buf.set_draining(true);
  EXPECT_FALSE(enqueue(buf, third));
  EXPECT_EQ(buf.count(), 1);
  EXPECT_EQ(buf.storage(), s.before);
  buf.set_draining(false);

  // Once the drain is over the same enqueue compacts and succeeds
  ASSERT_TRUE(enqueue(buf, third));
  EXPECT_EQ(buf.storage(), s.before);
  append(s.received, this->drain_all_(buf));
  expect_after_filler(s.received, s.filler, concat({s.first, s.second, third}));
}

TEST_F(OverflowBufferTest, CompactsInsteadOfGrowingAfterPartialDrain) {
  TestOverflowBuffer buf;
  Stall s;
  ASSERT_NO_FATAL_FAILURE(this->stall_mid_message_(buf, s));
  auto third = make_message(1000, 200);

  // The sent first message is reclaimed by sliding the remainder down, not by reallocating
  ASSERT_TRUE(enqueue(buf, third));
  EXPECT_EQ(buf.storage(), s.before);

  append(s.received, this->drain_all_(buf));
  expect_after_filler(s.received, s.filler, concat({s.first, s.second, third}));
}

}  // namespace esphome::api::testing
#endif  // USE_HOST
