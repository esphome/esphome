#include <algorithm>
#include "common.h"
#include "esphome/components/uart/uart.h"

namespace esphome::uart::testing {

// Feeds bytes from a fixed vector so available() and read_array() agree like a real UART.
class FakeRxUART : public MockUARTComponent {
 public:
  explicit FakeRxUART(std::vector<uint8_t> rx) : rx_(std::move(rx)) {
    ON_CALL(*this, available()).WillByDefault(Invoke([this]() { return this->rx_.size() - this->pos_; }));
    ON_CALL(*this, read_array(_, _)).WillByDefault(Invoke([this](uint8_t *data, size_t len) {
      this->read_sizes.push_back(len);
      if (this->fail_after_reads >= 0 && static_cast<int>(this->read_sizes.size()) > this->fail_after_reads)
        return false;
      if (len > this->rx_.size() - this->pos_)
        return false;
      std::memcpy(data, this->rx_.data() + this->pos_, len);
      this->pos_ += len;
      return true;
    }));
  }
  void append(std::initializer_list<uint8_t> more) { this->rx_.insert(this->rx_.end(), more); }
  size_t remaining() const { return this->rx_.size() - this->pos_; }

  std::vector<size_t> read_sizes;
  int fail_after_reads{-1};

 protected:
  std::vector<uint8_t> rx_;
  size_t pos_{0};
};

static std::vector<uint8_t> make_bytes(size_t n) {
  std::vector<uint8_t> v(n);
  for (size_t i = 0; i < n; i++)
    v[i] = static_cast<uint8_t>(i);
  return v;
}

TEST(UARTReadAvailableTest, NothingAvailableDoesNotRead) {
  ::testing::NiceMock<FakeRxUART> uart(std::vector<uint8_t>{});
  UARTDevice dev(&uart);
  int calls = 0;
  EXPECT_TRUE(dev.read_available([&calls](uint8_t) { calls++; }));
  EXPECT_EQ(calls, 0);
  EXPECT_TRUE(uart.read_sizes.empty());
}

TEST(UARTReadAvailableTest, ReadsEveryByteInChunks) {
  ::testing::NiceMock<FakeRxUART> uart(make_bytes(10));
  UARTDevice dev(&uart);
  std::vector<uint8_t> got;
  EXPECT_TRUE(dev.read_available<4>([&got](uint8_t b) { got.push_back(b); }));
  EXPECT_EQ(got, make_bytes(10));
  EXPECT_EQ(uart.read_sizes, (std::vector<size_t>{4, 4, 2}));
}

TEST(UARTReadAvailableTest, OnlyBytesAvailableOnEntry) {
  ::testing::NiceMock<FakeRxUART> uart(std::vector<uint8_t>{1, 2, 3});
  UARTDevice dev(&uart);
  std::vector<uint8_t> got;
  EXPECT_TRUE(dev.read_available<2>([&](uint8_t b) {
    got.push_back(b);
    if (b == 1)
      uart.append({9, 9});  // arrives while draining; left for the next call
  }));
  EXPECT_EQ(got, (std::vector<uint8_t>{1, 2, 3}));
  EXPECT_EQ(uart.remaining(), 2u);
}

TEST(UARTReadAvailableTest, CountReadsExactlyThatMany) {
  ::testing::NiceMock<FakeRxUART> uart(make_bytes(100));
  UARTDevice dev(&uart);
  size_t count = 0;
  EXPECT_TRUE(dev.read_available<16>([&count](uint8_t) { count++; }, 40));
  EXPECT_EQ(count, 40u);
  EXPECT_EQ(uart.read_sizes, (std::vector<size_t>{16, 16, 8}));
  EXPECT_EQ(uart.remaining(), 60u);
}

TEST(UARTReadAvailableTest, ReadFailureStopsAndReturnsFalse) {
  ::testing::NiceMock<FakeRxUART> uart(make_bytes(10));
  uart.fail_after_reads = 1;
  UARTDevice dev(&uart);
  std::vector<uint8_t> got;
  EXPECT_FALSE(dev.read_available<4>([&got](uint8_t b) { got.push_back(b); }));
  EXPECT_EQ(got, (std::vector<uint8_t>{0, 1, 2, 3}));
  EXPECT_EQ(uart.read_sizes.size(), 2u);
}

TEST(UARTReadAvailableTest, ByteCallbackReturningFalseStops) {
  ::testing::NiceMock<FakeRxUART> uart(make_bytes(10));
  UARTDevice dev(&uart);
  std::vector<uint8_t> got;
  EXPECT_TRUE(dev.read_available<4>([&got](uint8_t b) {
    got.push_back(b);
    return b != 5;
  }));
  // Stops mid chunk: bytes 6 and 7 were read from the UART but not handed over, 8 and 9 were never read.
  EXPECT_EQ(got, (std::vector<uint8_t>{0, 1, 2, 3, 4, 5}));
  EXPECT_EQ(uart.remaining(), 2u);
}

TEST(UARTReadAvailableTest, ChunkCallbackGetsWholeChunks) {
  ::testing::NiceMock<FakeRxUART> uart(make_bytes(70));
  UARTDevice dev(&uart);
  std::vector<size_t> lens;
  std::vector<uint8_t> got;
  EXPECT_TRUE(dev.read_available_chunks([&](const uint8_t *data, size_t len) {
    lens.push_back(len);
    got.insert(got.end(), data, data + len);
  }));
  EXPECT_EQ(lens, (std::vector<size_t>{64, 6}));
  EXPECT_EQ(got, make_bytes(70));
}

TEST(UARTReadAvailableTest, ChunkCallbackReturningFalseStops) {
  ::testing::NiceMock<FakeRxUART> uart(make_bytes(20));
  UARTDevice dev(&uart);
  int chunks = 0;
  EXPECT_TRUE(dev.read_available_chunks<8>([&chunks](const uint8_t *, size_t) {
    chunks++;
    return false;
  }));
  EXPECT_EQ(chunks, 1);
  EXPECT_EQ(uart.remaining(), 12u);
}

TEST(UARTReadAvailableTest, ChunksWithCountStopEarly) {
  ::testing::NiceMock<FakeRxUART> uart(make_bytes(30));
  UARTDevice dev(&uart);
  std::vector<size_t> lens;
  EXPECT_TRUE(dev.read_available_chunks<8>(
      [&lens](const uint8_t *, size_t len) {
        lens.push_back(len);
        return lens.size() < 2;
      },
      20));
  EXPECT_EQ(lens, (std::vector<size_t>{8, 8}));
  EXPECT_EQ(uart.remaining(), 14u);
}

TEST(UARTReadAvailableTest, DiscardDropsAvailableBytesInChunks) {
  ::testing::NiceMock<FakeRxUART> uart(make_bytes(40));
  UARTDevice dev(&uart);
  EXPECT_EQ(dev.discard_available(), 40u);
  EXPECT_EQ(uart.remaining(), 0u);
  EXPECT_EQ(uart.read_sizes, (std::vector<size_t>{16, 16, 8}));
}

TEST(UARTReadAvailableTest, DiscardWithNothingAvailableDoesNotRead) {
  ::testing::NiceMock<FakeRxUART> uart(std::vector<uint8_t>{});
  UARTDevice dev(&uart);
  EXPECT_EQ(dev.discard_available(), 0u);
  EXPECT_TRUE(uart.read_sizes.empty());
}

}  // namespace esphome::uart::testing
