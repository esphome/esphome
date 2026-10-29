#ifdef USE_HOST
#include "common.h"

// Keep host allocation/logging, and compile the actual ESP32-only implementation
// in this translation unit against the FreeRTOS API double.
#define USE_ESP32
// NOLINTNEXTLINE(bugprone-suspicious-include): exercise production code with the host FreeRTOS double.
#include "esphome/components/ring_buffer/ring_buffer.cpp"

namespace esphome::ring_buffer {
class RingBufferWrites : public ::testing::Test {
 protected:
  void SetUp() override { ringbuf_test::reset(); }
  std::unique_ptr<RingBuffer> filled_() {
    auto buffer = RingBuffer::create(8);
    const uint8_t initial[]{0, 1, 2, 3, 4, 5, 6, 7};
    EXPECT_EQ(buffer->write_without_replacement(initial, sizeof(initial)), sizeof(initial));
    ringbuf_test::reset();
    return buffer;
  }
};

TEST_F(RingBufferWrites, OversizedFullWriteReachesTheWaitPath) {
  auto buffer = this->filled_();
  const uint8_t data[32]{};
  EXPECT_EQ(buffer->write_without_replacement(data, sizeof(data), 20), 0);
  EXPECT_EQ(ringbuf_test::waits, std::vector<TickType_t>{20});
  ASSERT_FALSE(ringbuf_test::sends.empty());
  EXPECT_EQ(ringbuf_test::sends.front().length, 8);
  EXPECT_EQ(buffer->available(), 8);
}

TEST_F(RingBufferWrites, ReaderReleaseAllowsOversizedWriterToProgress) {
  auto buffer = this->filled_();
  ringbuf_test::on_wait = [&]() {
    uint8_t output[3];
    EXPECT_EQ(buffer->read(output, sizeof(output)), 3);
  };
  const uint8_t data[32]{8};
  EXPECT_EQ(buffer->write_without_replacement(data, sizeof(data), 20), 3);
  // One wait budget: the write of what fits after the wait does not wait again
  EXPECT_EQ(ringbuf_test::waits, std::vector<TickType_t>{20});
  EXPECT_EQ(ringbuf_test::sends.back().ticks, 0);
  uint8_t output[8];
  ASSERT_EQ(buffer->read(output, sizeof(output)), 8);
  EXPECT_EQ(std::vector<uint8_t>(output, output + 8), (std::vector<uint8_t>{3, 4, 5, 6, 7, 8, 0, 0}));
}

TEST_F(RingBufferWrites, OversizedWriteFillsTheFreeSpace) {
  auto buffer = this->filled_();
  uint8_t discarded[3];
  ASSERT_EQ(buffer->read(discarded, sizeof(discarded)), 3);
  ringbuf_test::reset();
  const uint8_t data[32]{8, 9, 10};
  EXPECT_EQ(buffer->write_without_replacement(data, sizeof(data), 20), 3);
  EXPECT_EQ(ringbuf_test::waits, std::vector<TickType_t>{20});
  ASSERT_EQ(ringbuf_test::sends.size(), 2);
  EXPECT_EQ(ringbuf_test::sends.back().length, 3);
  uint8_t output[8];
  ASSERT_EQ(buffer->read(output, sizeof(output)), 8);
  EXPECT_EQ(std::vector<uint8_t>(output, output + 8), (std::vector<uint8_t>{3, 4, 5, 6, 7, 8, 9, 10}));
}

TEST_F(RingBufferWrites, AtomicOversizedWriteStillFailsWithoutConsumingData) {
  auto buffer = this->filled_();
  const uint8_t data[32]{};
  EXPECT_EQ(buffer->write_without_replacement(data, sizeof(data), 20, false), 0);
  EXPECT_TRUE(ringbuf_test::waits.empty());
  ASSERT_EQ(ringbuf_test::sends.size(), 1);
  EXPECT_EQ(ringbuf_test::sends.front().length, sizeof(data));
  EXPECT_EQ(buffer->available(), 8);
}

TEST_F(RingBufferWrites, NormalWriteKeepsTheFastPath) {
  auto buffer = RingBuffer::create(8);
  const uint8_t data[]{1, 2, 3};
  EXPECT_EQ(buffer->write_without_replacement(data, sizeof(data), 20), sizeof(data));
  EXPECT_EQ(ringbuf_test::free_queries, 0);
  EXPECT_TRUE(ringbuf_test::waits.empty());
  ASSERT_EQ(ringbuf_test::sends.size(), 1);
}

TEST_F(RingBufferWrites, NonblockingAndEmptyWritesDoNotWait) {
  auto buffer = this->filled_();
  const uint8_t data[32]{};
  EXPECT_EQ(buffer->write_without_replacement(data, sizeof(data), 0), 0);
  EXPECT_EQ(buffer->write_without_replacement(nullptr, 0, 20), 0);
  EXPECT_TRUE(ringbuf_test::waits.empty());
  EXPECT_EQ(buffer->available(), 8);
}

TEST_F(RingBufferWrites, PartialWritesAndWrapPreserveEveryByte) {
  auto buffer = RingBuffer::create(7);
  std::vector<uint8_t> input(128), output;
  for (size_t i = 0; i < input.size(); ++i)
    input[i] = i;
  size_t position = 0;
  for (unsigned attempt = 0; attempt < 64 && (position < input.size() || buffer->available()); ++attempt) {
    position += buffer->write_without_replacement(input.data() + position, input.size() - position, 0);
    uint8_t chunk[4];
    const size_t count = buffer->read(chunk, sizeof(chunk));
    output.insert(output.end(), chunk, chunk + count);
  }
  EXPECT_EQ(output, input);
}
}  // namespace esphome::ring_buffer
#endif  // USE_HOST
