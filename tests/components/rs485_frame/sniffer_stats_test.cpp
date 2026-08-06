// USE_RS485_FRAME_SNIFFER_STATS is normally emitted by to_code only when a hub's YAML
// config has a sniffer_stats: block. rs485_frame is MULTI_CONF, so the unit-test harness's
// synthesized empty-list config never runs codegen for it at all (see
// script/build_helpers.py's populate_dependency_config), and the harness suppresses to_code
// by default anyway -- there's no per-instance call for a manifest override to hook, and no
// route to a real-config codegen path without also standing up a full uart+hub fixture that
// the harness can compile on host. Defining the macro directly (as
// tests/components/time/posix_tz_parser.cpp does for USE_TIME_TIMEZONE) only covers this
// translation unit, and sniffer_stats.cpp is a separate one -- so #include the real .cpp
// directly, compiling the actual implementation under the same macro in one unit.
#define USE_RS485_FRAME_SNIFFER_STATS

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/rs485_frame/sniffer_stats.cpp"  // NOLINT

namespace esphome::rs485_frame::testing {

namespace {

// Exposes the protected entries_ table so tests can inspect what record() actually
// captured, instead of scraping ESP_LOGI output.
class SnifferStatsProbe : public SnifferStats {
 public:
  size_t entry_count() const { return this->entries_.size(); }
  const SnifferEntry &entry(size_t i) const { return this->entries_[i]; }
};

}  // namespace

// rs485-zbj: a forum report claimed the periodic sniffer dump stayed capped at 32 bytes
// even with payload_capture_bytes: 48 set, despite dump_frames showing the full frame.
// This proves record()'s actual captured PayloadCapture::len/bytes for a payload longer
// than the historical 32-byte default, ruling out a capture-side truncation bug.
TEST(SnifferStatsTest, RecordCapturesFullPayloadPastLegacyThirtyTwoByteDefault) {
  SnifferStatsProbe stats;
  stats.init(/*max_entries=*/8, /*interval_ms=*/60000, /*payload_dump_top=*/4,
             /*max_unique_payloads=*/24, /*payload_capture_bytes=*/48,
             /*reference_frame_type=*/{}, /*strip_high_bit=*/true, /*reference_mode_send=*/false);

  // Reporter's real 35-byte Hayward display payload (decoded from their pasted dump_frames
  // hex): "\x01\x03 Filter Lo-all         to 05:30P\x00"
  const std::vector<uint8_t> payload = {0x01, 0x03, 0x20, 0x46, 0x69, 0x6c, 0x74, 0x65, 0x72, 0x20, 0x4c, 0x6f,
                                        0x2d, 0x61, 0x6c, 0x6c, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                        0x20, 0x74, 0x6f, 0x20, 0x30, 0x35, 0x3a, 0x33, 0x30, 0x50, 0x00};
  ASSERT_EQ(payload.size(), 35u);

  stats.record(payload, /*now=*/0);

  ASSERT_EQ(stats.entry_count(), 1u);
  const SnifferEntry &e = stats.entry(0);
  ASSERT_EQ(e.unique_count, 1u);
  const PayloadCapture &captured = e.payloads[0];
  EXPECT_EQ(captured.len, 35u) << "payload_capture_bytes=48 should not truncate a 35-byte payload";
  EXPECT_EQ(std::vector<uint8_t>(captured.bytes.get(), captured.bytes.get() + captured.len), payload);
}

}  // namespace esphome::rs485_frame::testing
