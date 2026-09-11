#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "esphome/components/api/api_overflow_buffer.h"
#include "esphome/components/socket/socket.h"

namespace esphome::api::testing {

// Exposes the storage so tests can check that it is reused rather than reallocated.
class TestOverflowBuffer : public APIOverflowBuffer {
 public:
  size_t capacity() const { return this->buf_.capacity(); }
  const uint8_t *storage() const { return this->buf_.data(); }
  uint8_t count() const { return this->count_; }
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

// A non-blocking unix socket pair with small buffers so the writer side can be
// filled deterministically, the way a stalled TCP connection behaves.
class OverflowBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int fds[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    int size = 4096;
    ::setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    ::setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    ::fcntl(fds[1], F_SETFL, O_NONBLOCK);
    this->reader_ = fds[1];
    this->sock_ = std::make_unique<socket::Socket>(fds[0]);
    ASSERT_EQ(this->sock_->setblocking(false), 0);
  }
  void TearDown() override { ::close(this->reader_); }

  /// Write filler until the socket refuses more; returns the number of filler bytes accepted.
  size_t fill_pipe() {
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

  /// Read whatever the pipe currently holds.
  std::vector<uint8_t> read_available() {
    std::vector<uint8_t> out;
    uint8_t tmp[1024];
    for (;;) {
      ssize_t n = ::read(this->reader_, tmp, sizeof(tmp));
      if (n <= 0)
        break;
      out.insert(out.end(), tmp, tmp + n);
    }
    return out;
  }

  /// Alternate reading and draining until the backlog is empty; returns all bytes received.
  std::vector<uint8_t> drain_all(TestOverflowBuffer &buf) {
    std::vector<uint8_t> received;
    while (!buf.empty()) {
      auto chunk = this->read_available();
      received.insert(received.end(), chunk.begin(), chunk.end());
      ssize_t sent = buf.try_drain(this->sock_.get());
      if (sent == -1) {
        EXPECT_TRUE(errno == EWOULDBLOCK || errno == EAGAIN);
      }
    }
    auto chunk = this->read_available();
    received.insert(received.end(), chunk.begin(), chunk.end());
    return received;
  }

  int reader_{-1};
  std::unique_ptr<socket::Socket> sock_;
};

TEST_F(OverflowBufferTest, IdleBufferOwnsNoStorage) {
  TestOverflowBuffer buf;
  EXPECT_TRUE(buf.empty());
  EXPECT_EQ(buf.capacity(), 0u);
  EXPECT_EQ(buf.storage(), nullptr);
}

TEST_F(OverflowBufferTest, StorageIsReusedAcrossStalls) {
  TestOverflowBuffer buf;
  auto msg = make_message(1000, 1);

  size_t filler = this->fill_pipe();
  ASSERT_TRUE(enqueue(buf, msg));
  const size_t capacity = buf.capacity();
  const uint8_t *storage = buf.storage();
  EXPECT_GE(capacity, msg.size() + 2);

  for (int stall = 0; stall < 5; stall++) {
    auto received = this->drain_all(buf);
    ASSERT_EQ(received.size(), filler + msg.size());
    EXPECT_TRUE(std::equal(msg.begin(), msg.end(), received.begin() + filler));
    EXPECT_TRUE(buf.empty());
    // Same allocation every time: no free, no new allocation
    EXPECT_EQ(buf.capacity(), capacity);
    EXPECT_EQ(buf.storage(), storage);

    filler = this->fill_pipe();
    ASSERT_TRUE(enqueue(buf, msg));
    EXPECT_EQ(buf.capacity(), capacity);
    EXPECT_EQ(buf.storage(), storage);
  }
  this->drain_all(buf);
}

TEST_F(OverflowBufferTest, ReleaseWhileQueuedFreesOnceDrained) {
  TestOverflowBuffer buf;
  auto msg = make_message(1000, 7);

  size_t filler = this->fill_pipe();
  ASSERT_TRUE(enqueue(buf, msg));
  const size_t capacity = buf.capacity();

  // Requested while the backlog still holds data: storage must stay until sent
  buf.release();
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.capacity(), capacity);

  auto received = this->drain_all(buf);
  ASSERT_EQ(received.size(), filler + msg.size());
  EXPECT_TRUE(std::equal(msg.begin(), msg.end(), received.begin() + filler));
  EXPECT_TRUE(buf.empty());
  EXPECT_EQ(buf.capacity(), 0u);
  EXPECT_EQ(buf.storage(), nullptr);

  // A later stall allocates again and keeps it, since nobody asked for a release
  filler = this->fill_pipe();
  ASSERT_TRUE(enqueue(buf, msg));
  EXPECT_GT(buf.capacity(), 0u);
  this->drain_all(buf);
  EXPECT_GT(buf.capacity(), 0u);
}

TEST_F(OverflowBufferTest, ReleaseWhenEmptyFreesImmediately) {
  TestOverflowBuffer buf;
  auto msg = make_message(100, 3);

  this->fill_pipe();
  ASSERT_TRUE(enqueue(buf, msg));
  this->drain_all(buf);
  EXPECT_GT(buf.capacity(), 0u);

  buf.release();
  EXPECT_EQ(buf.capacity(), 0u);
  EXPECT_EQ(buf.storage(), nullptr);
}

TEST_F(OverflowBufferTest, PreservesOrderAndSkipsSentPrefix) {
  TestOverflowBuffer buf;
  auto first = make_message(700, 10);
  auto second_a = make_message(300, 50);
  auto second_b = make_message(400, 90);
  auto third = make_message(200, 130);

  size_t filler = this->fill_pipe();
  // 100 bytes of the first message were already accepted by the socket
  ASSERT_TRUE(enqueue(buf, first, 100));
  // Two iovecs with the skip covering all of the first one plus part of the second
  struct iovec iov[2] = {{second_a.data(), second_a.size()}, {second_b.data(), second_b.size()}};
  const uint16_t second_skip = static_cast<uint16_t>(second_a.size() + 5);
  ASSERT_TRUE(buf.enqueue_iov(iov, 2, static_cast<uint16_t>(second_a.size() + second_b.size()), second_skip));
  ASSERT_TRUE(enqueue(buf, third));
  EXPECT_EQ(buf.count(), 3);

  // Nothing can go out while the pipe is full
  EXPECT_EQ(buf.try_drain(this->sock_.get()), -1);
  EXPECT_TRUE(errno == EWOULDBLOCK || errno == EAGAIN);
  EXPECT_EQ(buf.count(), 3);

  std::vector<uint8_t> expected;
  expected.insert(expected.end(), first.begin() + 100, first.end());
  expected.insert(expected.end(), second_b.begin() + 5, second_b.end());
  expected.insert(expected.end(), third.begin(), third.end());

  auto received = this->drain_all(buf);
  ASSERT_EQ(received.size(), filler + expected.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), received.begin() + filler));
}

TEST_F(OverflowBufferTest, RefusesWhenQueueIsFull) {
  TestOverflowBuffer buf;
  auto msg = make_message(16, 1);

  this->fill_pipe();
  for (int i = 0; i < API_MAX_SEND_QUEUE; i++) {
    ASSERT_TRUE(enqueue(buf, msg)) << "message " << i;
  }
  EXPECT_FALSE(enqueue(buf, msg));
  EXPECT_EQ(buf.count(), API_MAX_SEND_QUEUE);
  this->drain_all(buf);
}

TEST_F(OverflowBufferTest, CompactsInsteadOfGrowingAfterPartialDrain) {
  TestOverflowBuffer buf;
  size_t filler = this->fill_pipe();
  auto first = make_message(1500, 20);
  // Larger than the whole pipe, so a drain always stops part way through it
  auto second = make_message(filler * 3, 60);
  auto third = make_message(1000, 200);

  ASSERT_TRUE(enqueue(buf, first));
  ASSERT_TRUE(enqueue(buf, second));
  const size_t capacity = buf.capacity();
  const uint8_t *storage = buf.storage();

  auto received = this->read_available();
  ASSERT_GT(buf.try_drain(this->sock_.get()), 0);
  ASSERT_EQ(buf.count(), 1);

  // The sent first message is reclaimed by sliding the remainder down, not by reallocating
  ASSERT_TRUE(enqueue(buf, third));
  EXPECT_EQ(buf.capacity(), capacity);
  EXPECT_EQ(buf.storage(), storage);

  auto rest = this->drain_all(buf);
  received.insert(received.end(), rest.begin(), rest.end());
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), first.begin(), first.end());
  expected.insert(expected.end(), second.begin(), second.end());
  expected.insert(expected.end(), third.begin(), third.end());
  ASSERT_EQ(received.size(), filler + expected.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), received.begin() + filler));
}

}  // namespace esphome::api::testing
