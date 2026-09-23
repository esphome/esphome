// Unlike sniffer_stats_test.cpp/response_monitor_test.cpp/frame_trace_test.cpp, this file does
// NOT #include rs485_frame.cpp directly: that file is not macro-guarded as a whole (unlike the
// USE_RS485_FRAME_SNIFFER_STATS/RESPONSE_MONITOR/FRAME_TRACE-gated optional modules), so it is
// always compiled into the test binary's component library already -- doing so again here would
// duplicate-define every RS485FrameHub method. This test therefore links against that library
// object and only exercises paths that do not depend on USE_RS485_FRAME_FRAME_TRACE (the harness
// builds this component's library object with none of those optional macros defined, matching
// production builds where no hub in the synthesized test config requests those blocks) -- so
// frame_trace's own recording behavior is covered by frame_trace_test.cpp, and this file covers
// only the discard counter and the command_format guard that call into it.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/rs485_frame/rs485_frame.h"

namespace esphome::rs485_frame::testing {

namespace {

// Exposes just enough protected state/behavior to drive queue_command_values() and
// record_discard_() without standing up a real UART (RS485FrameHub::setup() reserves TX buffers
// and a real UARTComponent parent, neither of which either code path under test touches).
class RS485FrameHubProbe : public RS485FrameHub {
 public:
  // Mirrors the one piece of RS485FrameHub::setup() that enqueue_frame_() actually needs: a
  // non-empty tx_queue_ ring sized to max_queue_size_. Everything else setup() does (UART
  // buffer reservations, discovery) is unrelated to the paths under test.
  void prepare_queue_for_test() {
    this->tx_queue_.resize(this->max_queue_size_);
    for (auto &slot : this->tx_queue_)
      slot.reserve(32);
  }

  uint32_t command_drops_for_test() const { return this->command_drops_; }
  uint32_t discarded_frames_for_test() const { return this->discarded_frames_; }
  void record_discard_for_test(uint32_t now) { this->record_discard_(now); }
};

}  // namespace

// Review finding 9: queue_command_value(s) must refuse to encode against a hub with no
// command_format: rather than silently falling back to a 4-byte big-endian default -- those
// bytes would not match what the bus actually expects.
TEST(RS485FrameHubTest, QueueCommandValuesFailsWithoutCommandFormat) {
  RS485FrameHubProbe hub;

  EXPECT_FALSE(hub.queue_command_value(0x1234));
  EXPECT_EQ(hub.command_drops_for_test(), 1u);
}

// Same call succeeds once a command_format: is configured -- proves the new guard does not
// regress the normal, correctly-configured path.
TEST(RS485FrameHubTest, QueueCommandValuesSucceedsWithCommandFormat) {
  RS485FrameHubProbe hub;
  hub.set_command_format(/*preamble=*/{0x00, 0x83, 0x01}, /*value_element_bytes=*/1, /*big_endian=*/true,
                         /*postamble=*/{0x00});
  hub.prepare_queue_for_test();

  EXPECT_TRUE(hub.queue_command_value(0x01));
  EXPECT_EQ(hub.command_drops_for_test(), 0u);
}

// Review finding 10: a frame abandoned mid-receive (max_frame_length overflow or intra-frame
// timeout) must be counted separately from crc_failures -- it never reaches validate_frame_() at
// all, so folding it into that counter would misreport "CRC/structural failures" for something
// that was never even framed-and-checked.
TEST(RS485FrameHubTest, RecordDiscardIncrementsDiscardedFramesCounter) {
  RS485FrameHubProbe hub;

  hub.record_discard_for_test(/*now=*/1000);
  hub.record_discard_for_test(/*now=*/2000);

  EXPECT_EQ(hub.discarded_frames_for_test(), 2u);
}

}  // namespace esphome::rs485_frame::testing
