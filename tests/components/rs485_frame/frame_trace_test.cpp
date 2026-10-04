// USE_RS485_FRAME_FRAME_TRACE is normally emitted by to_code only when a hub's YAML config
// has a frame_trace: block -- see sniffer_stats_test.cpp's header comment for why this test
// #includes the real .cpp directly under the macro rather than relying on codegen.
#define USE_RS485_FRAME_FRAME_TRACE

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/rs485_frame/frame_trace.cpp"  // NOLINT

namespace esphome::rs485_frame::testing {

namespace {

// Exposes the protected ring state so tests can inspect exactly what record() stored,
// instead of scraping ESP_LOGI output from dump().
class FrameTraceProbe : public FrameTrace {
 public:
  size_t entry_count() const { return this->entries_.size(); }
  size_t capacity() const { return this->entries_.capacity(); }
  size_t write_index() const { return this->write_idx_; }
  const FrameTraceEntry &entry(size_t i) const { return this->entries_[i]; }
};

std::vector<uint8_t> make_frame(uint8_t first_byte, size_t len) {
  std::vector<uint8_t> frame(len);
  for (size_t i = 0; i < len; i++)
    frame[i] = static_cast<uint8_t>(first_byte + i);
  return frame;
}

}  // namespace

TEST(FrameTraceTest, RingWraparoundOverwritesOldestEntry) {
  FrameTraceProbe trace;
  trace.init(/*size=*/4, /*capture_bytes=*/8, /*include_invalid=*/true);

  // Record N+1 (5) frames into a 4-entry ring. Tag each frame's first payload byte with its
  // sequence number (10, 11, 12, ...) so the surviving entries are identifiable.
  for (uint8_t i = 0; i < 5; i++) {
    auto frame = make_frame(10 + i, 6);
    trace.record(frame.data(), frame.size(), /*is_tx=*/false, /*valid=*/true, /*now=*/1000 + i);
  }

  ASSERT_EQ(trace.entry_count(), 4u) << "ring capacity is 4; size must not grow past it";
  ASSERT_EQ(trace.capacity(), 4u);

  // Frame 0 (first_byte=10) was the oldest and must have been overwritten by frame 4
  // (first_byte=14). The write cursor wraps: after 5 records into a 4-slot ring, the next
  // write lands at index 1 (5 % 4), meaning slot 0 holds the 5th (most recent) frame.
  EXPECT_EQ(trace.write_index(), 1u);
  EXPECT_EQ(trace.entry(0).bytes[0], 14) << "slot 0 should hold the most recent overwrite";
  EXPECT_EQ(trace.entry(1).bytes[0], 11) << "slot 1..3 should still hold frames 1..3";
  EXPECT_EQ(trace.entry(2).bytes[0], 12);
  EXPECT_EQ(trace.entry(3).bytes[0], 13);
}

TEST(FrameTraceTest, CaptureBytesTruncatesButTotalLenReflectsOnWireLength) {
  FrameTraceProbe trace;
  trace.init(/*size=*/4, /*capture_bytes=*/5, /*include_invalid=*/true);

  auto frame = make_frame(0x20, 10);  // 10 bytes on the wire, capture_bytes caps storage at 5
  trace.record(frame.data(), frame.size(), /*is_tx=*/false, /*valid=*/true, /*now=*/42);

  ASSERT_EQ(trace.entry_count(), 1u);
  const FrameTraceEntry &e = trace.entry(0);
  EXPECT_EQ(e.total_len, 10u) << "total_len must reflect the actual on-wire length";
  EXPECT_EQ(e.captured_len, 5u) << "captured_len must be capped at capture_bytes";
  EXPECT_EQ(std::vector<uint8_t>(e.bytes.get(), e.bytes.get() + e.captured_len),
            std::vector<uint8_t>(frame.begin(), frame.begin() + 5));
}

TEST(FrameTraceTest, IncludeInvalidFalseSuppressesInvalidFrames) {
  FrameTraceProbe trace;
  trace.init(/*size=*/4, /*capture_bytes=*/8, /*include_invalid=*/false);

  auto valid_frame = make_frame(1, 4);
  auto invalid_frame = make_frame(2, 4);
  trace.record(valid_frame.data(), valid_frame.size(), /*is_tx=*/false, /*valid=*/true, /*now=*/1);
  trace.record(invalid_frame.data(), invalid_frame.size(), /*is_tx=*/false, /*valid=*/false, /*now=*/2);

  ASSERT_EQ(trace.entry_count(), 1u) << "the invalid frame must not have been recorded";
  EXPECT_TRUE(trace.entry(0).valid);
  EXPECT_EQ(trace.entry(0).bytes[0], 1);
}

TEST(FrameTraceTest, IncludeInvalidTrueCapturesAndTagsInvalidFrames) {
  FrameTraceProbe trace;
  trace.init(/*size=*/4, /*capture_bytes=*/8, /*include_invalid=*/true);

  auto valid_frame = make_frame(1, 4);
  auto invalid_frame = make_frame(2, 4);
  trace.record(valid_frame.data(), valid_frame.size(), /*is_tx=*/false, /*valid=*/true, /*now=*/1);
  trace.record(invalid_frame.data(), invalid_frame.size(), /*is_tx=*/false, /*valid=*/false, /*now=*/2);

  ASSERT_EQ(trace.entry_count(), 2u);
  EXPECT_TRUE(trace.entry(0).valid);
  EXPECT_FALSE(trace.entry(1).valid) << "invalid frames must be tagged distinctly from valid ones";
}

TEST(FrameTraceTest, TxAndRxAreTaggedPerEntry) {
  FrameTraceProbe trace;
  trace.init(/*size=*/4, /*capture_bytes=*/8, /*include_invalid=*/true);

  auto rx_frame = make_frame(1, 4);
  auto tx_frame = make_frame(2, 4);
  trace.record(rx_frame.data(), rx_frame.size(), /*is_tx=*/false, /*valid=*/true, /*now=*/1);
  trace.record(tx_frame.data(), tx_frame.size(), /*is_tx=*/true, /*valid=*/true, /*now=*/2);

  ASSERT_EQ(trace.entry_count(), 2u);
  EXPECT_FALSE(trace.entry(0).is_tx);
  EXPECT_TRUE(trace.entry(1).is_tx);
}

}  // namespace esphome::rs485_frame::testing
